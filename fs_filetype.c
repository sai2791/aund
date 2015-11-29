/*-
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
 * fs_filetype.c - guessing RISC OS file types
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <errno.h>
#include <regex.h>
#include <stdlib.h>

#include "extern.h"
#include "fileserver.h"

#define FT_DEVICE	0xfcc
#define FT_SOFTLINK	0xfdc
#define FT_UNIXEX	0xfe6
#define FT_DATA		0xffd
#define FT_TEXT		0xfff

enum fs_map_kind { FS_MAP_DEFAULT, FS_MAP_MODE, FS_MAP_NAME };

/* File type from name guess.  If the regexp matches, it has this type */
struct fs_typemap {
	TAILQ_ENTRY(fs_typemap) link;
	enum fs_map_kind kind;
	union {
		regex_t *name_re;
		struct {
			mode_t val;
			mode_t mask;
		} mode;
	} crit;
	int type;
};

TAILQ_HEAD(fs_typemap_head, fs_typemap);
static struct fs_typemap_head typemap = TAILQ_HEAD_INITIALIZER(typemap);

static int fs_check_typemap(FTSENT *, struct fs_typemap *);

/*
 * fs_guess_type - pick a sensible RISC OS file type for a Unix file.
 */
int fs_guess_type(FTSENT *f)
{
	struct fs_typemap *map;
	
	/* First check for magic names */
	if (f->fts_namelen >= 4 && f->fts_name[f->fts_namelen-4] == ',')
		/* XXX should support ,xxx and ,lxa */
		return strtoul(f->fts_name + f->fts_namelen - 3, NULL, 16);
	for (map = typemap.tqh_first; map != NULL; map = map->link.tqe_next)
		if (fs_check_typemap(f, map)) break;

	if (map) return map->type;
	/* Urgh.  Do we do content-based guessing, or just give up? */
	return FT_DATA;
}

int fs_check_typemap(FTSENT *f, struct fs_typemap *map)
{
	switch (map->kind) {
	case FS_MAP_DEFAULT:
		return 1;
	case FS_MAP_MODE:
		return (f->fts_statp->st_mode & map->crit.mode.mask) ==
		    map->crit.mode.val;
	case FS_MAP_NAME:
		if (regexec(map->crit.name_re, f->fts_name, 0, NULL, 0) == 0)
			return 1;
		else
			return 0;
	}
	return 0;
}

int
fs_add_typemap_name(const char *re, int type)
{
	struct fs_typemap *newmap;
	int rc;
	
	newmap = malloc(sizeof(*newmap));
	if (newmap == NULL) {
		errno = ENOMEM;
		return -1;
	}
	newmap->crit.name_re = malloc(sizeof(*(newmap->crit.name_re)));
	if (newmap->crit.name_re == NULL) {
		free(newmap);
		errno = ENOMEM;
		return -1;
	}
	if ((rc =
		regcomp(newmap->crit.name_re, re, REG_EXTENDED | REG_NOSUB))) {
		free(newmap->crit.name_re);
		free(newmap);
		errno = 0;
		return -1;
	}
	newmap->type = type;
	newmap->kind = FS_MAP_NAME;
	TAILQ_INSERT_TAIL(&typemap, newmap, link);
	return 0;
}

int
fs_add_typemap_mode(mode_t val, mode_t mask, int type)
{
	struct fs_typemap *newmap;
	
	newmap = malloc(sizeof(*newmap));
	if (newmap == NULL) {
		errno = ENOMEM;
		return -1;
	}
	newmap->crit.mode.val = val;
	newmap->crit.mode.mask = mask;
	newmap->type = type;
	newmap->kind = FS_MAP_MODE;
	TAILQ_INSERT_TAIL(&typemap, newmap, link);
	return 0;
}
int
fs_add_typemap_default(int type)
{
	struct fs_typemap *newmap;
	
	newmap = malloc(sizeof(*newmap));
	if (newmap == NULL) {
		errno = ENOMEM;
		return -1;
	}
	newmap->type = type;
	newmap->kind = FS_MAP_DEFAULT;
	TAILQ_INSERT_TAIL(&typemap, newmap, link);
	return 0;
}
