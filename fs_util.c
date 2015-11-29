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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "aun.h"
#include "extern.h"
#include "fs_proto.h"
#include "fileserver.h"

char *
strpad(char *s, int c, size_t len)
{
	int i;

	for (i = strlen(s); i < len; i++)
		s[i] = c;
	return s;
}

uint8_t
fs_mode_to_type(mode_t mode)
{

	if (S_ISDIR(mode)) return EC_FS_TYPE_DIR;
	return EC_FS_TYPE_FILE;
	/* Old clients may prefer EC_FS_TYPE_SOME */
}

/*
 * Conversions between Acorn access and Unix modes.  Acorn 'L'
 * prevents an object being deleted and has no Unix equivalent.
 * usergroup determines whether group permissions follow user or
 * public permissions when changed by an Acorn client.
 */

uint8_t
fs_mode_to_access(mode_t mode)
{
	unsigned char access;

	access = 0;
	if (mode & S_IRUSR) access |= EC_FS_ACCESS_UR;
	if (mode & S_IWUSR) access |= EC_FS_ACCESS_UW;
	if (mode & S_IROTH) access |= EC_FS_ACCESS_OR;
	if (mode & S_IWOTH) access |= EC_FS_ACCESS_OW;
	if (S_ISDIR(mode))  access |= EC_FS_ACCESS_D;
	return access;
}

mode_t
fs_access_to_mode(unsigned char access, int usergroup)
{
	mode_t mode;

	mode = 0;
	if (access & EC_FS_ACCESS_UR)
		mode |= S_IRUSR | (usergroup ? S_IRGRP : 0);
	if (access & EC_FS_ACCESS_UW)
		mode |= S_IWUSR | (usergroup ? S_IWGRP : 0);
	if (access & EC_FS_ACCESS_OR)
		mode |= S_IROTH | (usergroup ? 0 : S_IRGRP);
	if (access & EC_FS_ACCESS_OW)
		mode |= S_IWOTH | (usergroup ? 0 : S_IWGRP);
	return mode;
}

char *
fs_access_to_string(char *buf, uint8_t access)
{

	buf[0] = '\0';
	/*
	 * FIXME: this should take account of whether you own the
	 * file. (In an ideal world where file ownership is
	 * supported, that is.)
	 */
	if (access & EC_FS_ACCESS_D) strcat(buf, "D");
	if (access & EC_FS_ACCESS_L) strcat(buf, "L");
	if (access & EC_FS_ACCESS_UW) strcat(buf, "W");
	if (access & EC_FS_ACCESS_UR) strcat(buf, "R");
	strcat(buf, "/");
	if (access & EC_FS_ACCESS_OW) strcat(buf, "w");
	if (access & EC_FS_ACCESS_OR) strcat(buf, "r");
	return buf;
}

uint64_t
fs_read_val(uint8_t *p, size_t len)
{
	uint64_t value;

	value = 0;
	p += len - 1;
	while(len) {
		value <<= 8;
		value |= *p;
		p--;
		len--;
	}
	return value;
}

void
fs_write_val(uint8_t *p, uint64_t value, size_t len)
{
	uint64_t max;

	max = (1ULL << (len * 8)) - 1;
	if (value > max) value = max;
	while (len) {
		/* LINTED no loss of accuracy */
		*p = value & 0xff;
		p++;
		len--;
		value >>= 8;
	}
}

/*
 * Construct path to Acorn metadata for a file.  The caller must free the
 * returned string itself.
 */
static char *
fs_metapath(FTSENT *f)
{
	char *lastslash, *metapath;

	lastslash = strrchr(f->fts_accpath, '/');
	if (lastslash)
		lastslash++;
	else
		lastslash = f->fts_accpath;
	metapath = malloc((lastslash - f->fts_accpath) +
			  f->fts_namelen + 8 + 1);
	if (metapath != NULL) {
		sprintf(metapath, "%.*s.Acorn/%s",
		    (int)(lastslash - f->fts_accpath),
		    f->fts_accpath, f->fts_name);
	}
	return metapath;
}

void
fs_get_meta(FTSENT *f, struct ec_fs_meta *meta)
{
	struct stat *st;
	char *metapath, rawinfo[24];
	uint64_t stamp;
	int type, i, ret;

	metapath = fs_metapath(f);
	if (metapath != NULL) {
 		rawinfo[23] = '\0';
		ret = readlink(metapath, rawinfo, 23);
		if (ret == 23) {
			for (i = 0; i < 4; i++)
				/* LINTED strtoul result < 0x100 */
				meta->load_addr[i] =
				    strtoul(rawinfo+i*3, NULL, 16);
			for (i = 0; i < 4; i++)
				/* LINTED strtoul result < 0x100 */
				meta->exec_addr[i] =
				    strtoul(rawinfo+12+i*3, NULL, 16);
			return;
		} else if (ret == 17) {
			fs_write_val(meta->load_addr,
			    strtoul(rawinfo, NULL, 16),
			    sizeof(meta->load_addr));
			fs_write_val(meta->exec_addr,
			    strtoul(rawinfo + 9, NULL, 16),
			    sizeof(meta->load_addr));
			return;
		}
		free(metapath);
	}
	st = f->fts_statp;
	if (st != NULL) {
		stamp = fs_riscos_date(st->st_mtime,
#if HAVE_STRUCT_STAT_ST_MTIMENSEC
		    st->st_mtimensec / 10000000
#elif HAVE_STRUCT_STAT_ST_MTIM
		    st->st_mtim.tv_nsec / 10000000
#else
		    0
#endif
		    );
		type = fs_guess_type(f);
		fs_write_val(meta->load_addr,
			     0xfff00000 | (type << 8) | (stamp >> 32), 4);
		fs_write_val(meta->exec_addr, stamp & 0x00ffffffffULL, 4);
	} else {	
		fs_write_val(meta->load_addr, 0xdeaddead, 4);
		fs_write_val(meta->exec_addr, 0xdeaddead, 4);
	}
}

int
fs_set_meta(FTSENT *f, struct ec_fs_meta *meta)
{
	char *lastslash, *metapath, rawinfo[24];
	int ret;

	metapath = fs_metapath(f);
	if (metapath == NULL) {
		errno = ENOMEM;
		return 0;
	}

	lastslash = strrchr(metapath, '/');
	*lastslash = '\0'; /* metapath now points to the .Acorn directory. */
	ret = rmdir(metapath);
	if (ret < 0 && errno != ENOENT && errno != ENOTEMPTY)
		goto fail;
	if ((ret < 0 && errno == ENOENT) || ret == 0) {
		if (mkdir(metapath, 0777) < 0)
			goto fail;
	}
	*lastslash = '/'; /* metapath now points to the metadata again. */
	sprintf(rawinfo, "%08lX %08lX",
	    (unsigned long)
	    fs_read_val(meta->load_addr, sizeof(meta->load_addr)),
	    (unsigned long)
	    fs_read_val(meta->exec_addr, sizeof(meta->exec_addr)));
	if (unlink(metapath) < 0 && errno != ENOENT)
		goto fail;
	if (symlink(rawinfo, metapath) < 0)
		goto fail;
	free(metapath);
	return 1;
fail:
	free(metapath);
	return 0;
}

void
fs_del_meta(FTSENT *f)
{
	char *metapath;

	metapath = fs_metapath(f);
	if (metapath != NULL) {
		unlink(metapath);
		*strrchr(metapath, '/') = '\0';
		rmdir(metapath); /* Don't worry if it fails. */
		free(metapath);
	}
}

/*
 * Return the System Internal Name for a file.  This is only 24 bits long
 * but is expected to be unique across the whole disk.  For now, we fake
 * it with the bottom 24 bits of the inode number, which is less than
 * optimal.
 */
int
fs_get_sin(FTSENT *f)
{

	return f->fts_statp->st_ino & 0xFFFFFF;
}

/*
 * Get the creation time of a file, or the best approximation we can
 * manage.  Various bits of protocol return this as a fileserver date
 * or as a string.
 */
time_t
fs_get_birthtime(FTSENT *f)
{

#if HAVE_STRUCT_STAT_ST_BIRTHTIME
	/*
	 * NetBSD 5.0 seems to be confused over whether an unknown
	 * birthtime should be 0 or VNOVAL (-1).
	 */
	if (f->fts_statp->st_birthtime &&
	    f->fts_statp->st_birthtime != (time_t)(-1))
		return f->fts_statp->st_birthtime;
#endif
	/* Ah well, mtime will have to do. */
	return f->fts_statp->st_mtime;
}

/*
 * Convert a Unix time_t (non-leap seconds since 1970-01-01) and odd
 * centiseconds to a RISC OS time (non-leap(?) centiseconds since
 * 1900-01-01(?)).
 */

uint64_t
fs_riscos_date(time_t time, unsigned csec)
{
	uint64_t base;

	base = 31536000ULL * 70 + 86400 * 17;
	return (((uint64_t)time) + base)*100;
}

/*
 * Convert a date stamp from Unix to Acorn fileserver.
 */
void
fs_write_date(struct ec_fs_date *date, time_t time)
{
	struct tm *t;
	int year81;

	t = localtime(&time);
	if (t->tm_year < 81) {
		/* Too early -- return lowest date possible */
		date->day = 1;
		date->year_month = 1;
	} else {
		year81 = t->tm_year - 81;
		date->day = t->tm_mday | ((year81 & 0xf0) << 1);
		date->year_month = (t->tm_mon + 1) | (year81 << 4);
	}
}

/*
 * Mostly like stat(2), but if called on a broken symlink, returns
 * information on the symlink itself.
 */
int
fs_stat(const char *path, struct stat *sb)
{
	int rc;

	rc = stat(path, sb);
	if (rc == -1 && errno == ENOENT)
		/* Could be a broken symlink */
		rc = lstat(path, sb);
	return rc;
}

const char *
fs_leafname(const char *path)
{
	char *leaf;
	if ((leaf = strrchr(path,'/')) != NULL)
		return leaf+1;
	else
		return path;
}
