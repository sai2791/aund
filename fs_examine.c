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
/*
 * This is part of aund, an implementation of Acorn Universal
 * Networking for Unix.
 */	
/*
 * fs_examine.c - the Examine call (code 3) - Directory listing.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fts.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"
#include "fileserver.h"
#include "fs_errors.h"

static int fs_examine_read(struct fs_context *, const char *, int);

static int fs_examine_all(FTSENT *, struct ec_fs_reply_examine **, size_t *);
static int fs_examine_longtxt(struct fs_context *c, FTSENT *, struct ec_fs_reply_examine **,
    size_t *);
static int fs_examine_name(FTSENT *, struct ec_fs_reply_examine **, size_t *);
static int fs_examine_shorttxt(FTSENT *, struct ec_fs_reply_examine **,
    size_t *);

void
fs_examine(struct fs_context *c)
{
	/* LINTED subclass */
	struct ec_fs_req_examine *request = (struct ec_fs_req_examine *)(c->req);
	char *upath;
	FTSENT *ent;
	struct ec_fs_reply_examine *reply;
	size_t reply_size;
	int i, rc;

	request->path[strcspn(request->path, "\r")] = '\0';	
	if (debug)
		printf("examine [%d, %d/%d, %s]\n",
		    request->arg, request->start, request->nentries,
		    request->path);
	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	switch (request->arg) {
	default:
		fs_err(c, EC_FS_E_BADEXAMINE);
		return;
	case EC_FS_EXAMINE_ALL: case EC_FS_EXAMINE_NAME:
	case EC_FS_EXAMINE_SHORTTXT: case EC_FS_EXAMINE_LONGTXT:
		break;
	}
	upath = fs_unixify_path(c, request->path);
	if (upath == NULL) return;
	errno = 0;
	reply_size = sizeof(*reply);
	if (request->arg == EC_FS_EXAMINE_SHORTTXT ||
	    request->arg == EC_FS_EXAMINE_LONGTXT)
		reply = malloc(reply_size+1);
	else
		reply = malloc(reply_size);
	if (fs_examine_read(c, upath, request->start) == -1 || reply == NULL) {
		free(reply);
		free(upath);
		if (errno)
			fs_errno(c);
		else
			fs_err(c, EC_FS_E_NOMEM);
		return;
	}
	ent = c->client->dir_cache.f;
	for (i = c->client->dir_cache.start;
	     i < request->start && ent != NULL;
	     ent = ent->fts_link) {
		if (fs_hidden_name(ent->fts_name))
			continue;      /* hidden file */
		i++;		       /* count this one */
	}
	for (i = 0;
	     i < request->nentries && ent != NULL;
	     ent = ent->fts_link) {
		switch (ent->fts_info) {
		case FTS_ERR: case FTS_NS: /* FTS_DNR doesn't matter here */
			continue;
		}
		if (fs_hidden_name(ent->fts_name))
			continue;      /* hidden file */
		i++;
		switch (request->arg) {
		case EC_FS_EXAMINE_ALL:
			rc = fs_examine_all(ent, &reply, &reply_size);
			break;
		case EC_FS_EXAMINE_LONGTXT:
			rc = fs_examine_longtxt(c, ent, &reply, &reply_size);
			break;
		case EC_FS_EXAMINE_NAME:
			rc = fs_examine_name(ent, &reply, &reply_size);
			break;
		case EC_FS_EXAMINE_SHORTTXT:
			rc = fs_examine_shorttxt(ent, &reply, &reply_size);
			break;
		default:
			rc = -1; /* Cheer up gcc */
		}
		if (rc == -1)
			goto bye;
	}
bye:		
	reply->nentries = i;
	reply->undef0 = 0; /* What is this for? */
	reply->std_tx.command_code = EC_FS_CC_DONE;
	reply->std_tx.return_code = EC_FS_RC_OK;
	switch (request->arg) {
	case EC_FS_EXAMINE_LONGTXT: case EC_FS_EXAMINE_SHORTTXT:
		/* space for this is reserved */
		((unsigned char*)reply)[reply_size] = 0x80;
		reply_size++;
	}
	fs_reply(c, &(reply->std_tx), reply_size);
	/* If there's more to read, leave a useful cache. */
	if (ent != NULL) {
		c->client->dir_cache.f = ent;
		c->client->dir_cache.start = request->start + i;
	} else {
		fts_close(c->client->dir_cache.ftsp);
		c->client->dir_cache.ftsp = NULL;
		free(c->client->dir_cache.path);
		c->client->dir_cache.path = NULL;
	}
	free(reply);
	free(upath);
}

static int
fs_filename_compare(const FTSENT **a, const FTSENT **b)
{

	return strcasecmp((*a)->fts_name, (*b)->fts_name);
}

static int
fs_examine_read(struct fs_context *c, const char *upath, int start)
{
	char *path_argv[2];
	struct fs_dir_cache *dc;
        FTSENT *dir;
	
	dc = &(c->client->dir_cache);
	if (dc->path && strcmp(dc->path, upath) == 0 && dc->start == start) {
		/* Already cached */
		/* XXX this should see how recent the cache is */
		/* XXX Won't spot if the client skipped a bit of a listing */
		if (debug) printf("cache HIT!\n");
		return 0;
	}
	if (debug)
		printf("cache miss.  wanted %d; found %d.\n", start, dc->start);
	if (dc->ftsp)
		/* Dispose of old FTS structure */
		fts_close(dc->ftsp);
	free(dc->path);
	dc->path = strdup(upath);
	if (dc->path == NULL) {
		errno = ENOMEM;
		return -1;
	}

	path_argv[0] = dc->path;
	path_argv[1] = NULL;
        dc->ftsp = fts_open(path_argv, FTS_LOGICAL, fs_filename_compare);
	if (dc->ftsp == NULL) {
		free(dc->path); dc->path = NULL;
		return -1;
	}
	dir = fts_read(dc->ftsp); /* Returns the directory itself */
	if (dir == NULL) {
		fts_close(dc->ftsp); dc->ftsp = NULL;
		free(dc->path); dc->path = NULL;
		return -1;
	}
	switch (dir->fts_info) {
	case FTS_ERR: case FTS_DNR: case FTS_NS:
		fts_close(dc->ftsp); dc->ftsp = NULL;
		free(dc->path); dc->path = NULL;
		return -1;
	case FTS_D: case FTS_DC: case FTS_DP:
		break;
	default:
		fts_close(dc->ftsp); dc->ftsp = NULL;
		free(dc->path); dc->path = NULL;
		errno = ENOTDIR;
		return -1;
	}
	dc->f = fts_children(dc->ftsp, 0);
	dc->start = 0;
	return 0;
}

static int
fs_examine_all(FTSENT *ent, struct ec_fs_reply_examine **replyp,
    size_t *reply_sizep)
{
	struct ec_fs_exall *exall;
	void *new_reply;
	
	if ((new_reply = realloc(*replyp, *reply_sizep + sizeof(*exall)))
	    != NULL)
		*replyp = new_reply;
	if (new_reply == NULL) {
		errno = ENOMEM;
		goto burn;
	}
	exall = (struct ec_fs_exall *)(((void *)*replyp) + *reply_sizep);
	fs_get_meta(ent, &(exall->meta)); /* This needs the name unmodified */
	fs_acornify_name(ent->fts_name);
	strncpy(exall->name, ent->fts_name, sizeof(exall->name));
	strpad(exall->name, ' ', sizeof(exall->name));
	exall->access = fs_mode_to_access(ent->fts_statp->st_mode);
	fs_write_date(&(exall->date), fs_get_birthtime(ent));
	fs_write_val(exall->sin, fs_get_sin(ent), sizeof(exall->sin));
	fs_write_val(exall->size, ent->fts_statp->st_size,
		     sizeof(exall->size));
	*reply_sizep += sizeof(*exall);
	return 0;
burn:
	return -1;
}

static int
fs_examine_name(FTSENT *ent, struct ec_fs_reply_examine **replyp,
    size_t *reply_sizep)
{
	struct ec_fs_exname *exname;
	void *new_reply;
	
	if ((new_reply = realloc(*replyp, *reply_sizep + sizeof(*exname))) !=
	    NULL)
		*replyp = new_reply;
	if (new_reply == NULL) {
		errno = ENOMEM;
		goto burn;
	}
	exname = (struct ec_fs_exname *)(((void *)*replyp) + *reply_sizep);
	exname->namelen = sizeof(exname->name);
	fs_acornify_name(ent->fts_name);
	strncpy(exname->name, ent->fts_name, sizeof(exname->name));
	strpad(exname->name, ' ', sizeof(exname->name));
	*reply_sizep += sizeof(*exname);
	return 0;
burn:
	return -1;
}

static int
fs_examine_shorttxt(FTSENT *ent, struct ec_fs_reply_examine **replyp,
    size_t *reply_sizep)
{
	void *new_reply;
	char accstring[8];
	
	if ((new_reply = realloc(*replyp, *reply_sizep + 10+1+7+2)) != NULL)
		*replyp = new_reply;
	if (new_reply == NULL) {
		errno = ENOMEM;
		goto burn;
	}
	fs_acornify_name(ent->fts_name);
	fs_access_to_string(accstring,
	    fs_mode_to_access(ent->fts_statp->st_mode));
	sprintf((char*)(((void *)*replyp) + *reply_sizep), "%-10.10s %-7.7s",
	    ent->fts_name, accstring);
	*reply_sizep += 10+1+7+1; /* one byte spare to terminate */
	return 0;
burn:
	return -1;
}

static int
fs_examine_longtxt(struct fs_context *c, FTSENT *ent,
    struct ec_fs_reply_examine **replyp, size_t *reply_sizep)
{
	void *new_reply;
	char *string;
	
	if ((new_reply = realloc(*replyp, *reply_sizep + 100)) != NULL)
		*replyp = new_reply;
	if (new_reply == NULL) {
		errno = ENOMEM;
		goto burn;
	}
	string = (char*)(((void *)*replyp) + *reply_sizep);
	fs_long_info(c, string, ent);
	string[strcspn(string, "\r\x80")] = '\0';
	*reply_sizep += 1 + strlen(string); /* one byte spare to terminate */
	return 0;
burn:
	return -1;
}
