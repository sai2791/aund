/*-
 * Copyright (c) 2010 Simon Tatham
 * Copyright (c) 1998, 2010 Ben Harris
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "fileserver.h"

#define MAX_HANDLES 256

static int fs_alloc_handle(struct fs_client *, bool);
static void fs_free_handle(struct fs_client *, int);

/*
 * Check a client context for validity.  Zero invalid handles.
 */
void fs_check_handles(struct fs_context *c)
{
	if (debug) printf("{");
	switch (c->req->function) {
	default:
		if (debug) printf("&=%u,", c->req->urd);
		c->req->urd = fs_check_handle(c->client, c->req->urd);
		/* FALLTHROUGH */
	case EC_FS_FUNC_LOAD:
	case EC_FS_FUNC_LOAD_COMMAND:
	case EC_FS_FUNC_SAVE:
	case EC_FS_FUNC_GETBYTES:
	case EC_FS_FUNC_PUTBYTES:
		/* In these calls, the URD is replaced by a port number */
		if (debug) printf("@=%u,%%=%u", c->req->csd, c->req->lib);
		c->req->csd = fs_check_handle(c->client, c->req->csd);
		c->req->lib = fs_check_handle(c->client, c->req->lib);
		/* FALLTHROUGH */
	case EC_FS_FUNC_GETBYTE:
	case EC_FS_FUNC_PUTBYTE:
		/* And these ones don't pass context at all. */
		break;
	}
	if (debug) printf("} ");
}

/*
 * Check a handle for validity.  Return the handle if it's valid and 0
 * if it isn't.
 */
int fs_check_handle(struct fs_client *client, int h)
{
	if (client && h < client->nhandles && client->handles[h])
		return h;
	else
		return 0;
}

/*
 * Open a new handle for a client.  path gives the Unix path of the
 * file or directory to open.
 */
int
fs_open_handle(struct fs_client *client, char *path, int open_flags,
    bool for_open)
{
	struct stat sb;
	char *newpath;
	int h, fd;

	h = fs_alloc_handle(client, for_open);
	if (h == 0) {
		errno = EMFILE;
		return h;
	}
	if ((fd = open(path, open_flags,
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) == -1) {
		fs_free_handle(client, h);
		return 0;
	}
	if (fstat(fd, &sb) == -1) {
		close(fd);
		fs_free_handle(client, h);
		return 0;
	}
	if (S_ISDIR(sb.st_mode))
		client->handles[h]->type = FS_HANDLE_DIR;
	else if (S_ISREG(sb.st_mode)) {
		client->handles[h]->type = FS_HANDLE_FILE;
		/*
		 * Initialise the sequence number to 'unknown', so
		 * that the first request from the client will not
		 * be considered a repeat regardless of its sequence
		 * number.
		 */
		client->handles[h]->sequence = 0xFF;
		client->handles[h]->oldoffset = 0;
	} else {
		warnx("fs_open_handle: tried to open something odd");
		close(fd);
		fs_free_handle(client, h);
		errno = ENOENT;
		return 0;
	}
	client->handles[h]->fd = fd;
	newpath = client->handles[h]->path = malloc(strlen(path)+1);
	if (newpath == NULL) {
		warnx("fs_open_handle: malloc failed");
		close(fd);
		fs_free_handle(client, h);
		errno = ENOMEM;
		return 0;
	}
	strcpy(newpath, path);
	if (newpath[strlen(newpath)-1] == '/')
		newpath[strlen(newpath)-1] = '\0';
	if (debug) printf("{%d=%s} ", h, newpath);
	return h;
}

/*
 * Release a handle set up by fs_open_handle.
 */

void
fs_close_handle(struct fs_client *client, int h)
{

	if (h == 0) return;
	if (debug) printf("{%d closed} ", h);
	close(client->handles[h]->fd);
	free(client->handles[h]->path);
	fs_free_handle(client, h);
}

/*
 * Handle allocation is slightly tricksy owing to strange behaviour on
 * the part of early 8-bit clients (up to NFS 3.60 at least).  These
 * require that any handle on which they might do I/O be a power of
 * two, because they keep all the sequence number flags in a single
 * byte and mask out the correct one using the handle.  This restriction
 * doesn't apply to handles opened using *DIR etc, since these are
 * never used for I/O.
 *
 * As a result, we try to ensure that handles allocated by "open" are
 * powers of two, and that ones allocated by other means are not (to
 * preserve our precious stocks of power-of-two handles.  If the
 * safehandles option is turned on, the restriction on handles
 * allocated by "open" is enforced.
 *
 * Handle 255 is treated specially by NFS 3.60: it sends that handle
 * to the server whenever it's asked for one that it knows cannot
 * exist.  For this reason, when safehandles is enabled we avoid
 * allocating that one, even for a directory.
 *
 * Newer clients (BBC Master onwards) don't have any of these
 * restrictions, so for them the safehandles option can be turned off.
 */

/* Allocate a handle that's a power of two. */
static int
fs_alloc_handle_p2(struct fs_client *client)
{
	int h;

	for (h = 1; h < MAX_HANDLES; h <<= 1)
		if (h >= client->nhandles ||
		    client->handles[h] == NULL) return h;
	return 0;
}

/* Allocate a handle that's not a power of two. */
static int
fs_alloc_handle_np2(struct fs_client *client)
{
	int h;

	for (h = 1; h < MAX_HANDLES; h++)
		if ((h & (h - 1)) != 0 &&
		    (h >= client->nhandles ||
		     client->handles[h] == NULL)) return h;
	return 0;
}

/* Allocate handle 255. */
static int
fs_alloc_handle_255(struct fs_client *client)
{

	if (255 >= client->nhandles ||
	    client->handles[255] == NULL) return 255;
	return 0;
}

static int
fs_alloc_handle(struct fs_client *client, bool for_open)
{
	int h;

	if (for_open) {
		h = fs_alloc_handle_p2(client);
		if (!client->safehandles) {
			if (h == 0) h = fs_alloc_handle_np2(client);
			if (h == 0) h = fs_alloc_handle_255(client);
		}
	} else {
		h = fs_alloc_handle_np2(client);
		if (h == 0) h = fs_alloc_handle_p2(client);
		if (!client->safehandles && h == 0)
			h = fs_alloc_handle_255(client);
	}
	if (h == 0) return 0;
	if (h >= client->nhandles) {
		/* Extend the table. */
		int new_nhandles, i;
		struct fs_handle **new_handles;

		new_nhandles = h + 1;
		if (new_nhandles > MAX_HANDLES)
			new_nhandles = MAX_HANDLES;
		new_handles = realloc(client->handles,
		    new_nhandles * sizeof(struct fs_handle *));
		if (new_handles != NULL) {
			for (i = client->nhandles; i < new_nhandles; i++)
				new_handles[i] = NULL;
			client->handles = new_handles;
			client->nhandles = new_nhandles;
		} else {
			warnx("fs_alloc_handle: realloc failed");
			return 0;
		}
	}
	client->handles[h] = malloc(sizeof(*(client->handles[h])));
	if (client->handles[h] == NULL) {
		warnx("fs: fs_alloc_handle: malloc failed");
		h = 0;
	}
	return h;
}

static void
fs_free_handle(struct fs_client *client, int h)
{

	/* very simple */
	free(client->handles[h]);
	client->handles[h] = 0;
}

