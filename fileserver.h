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

#ifndef _FILESERVER_H
#define _FILESERVER_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <dirent.h>
#include <fts.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "aun.h"
#include "fs_proto.h"

struct fs_context {
	struct ec_fs_req *req;		/* Request being handled */
	size_t req_len;			/* Size of request */
	struct aun_srcaddr *from;	/* Source of request */
	struct fs_client *client;	/* Pointer to client structure, or NULL if not logged in */
};

enum fs_handle_type { FS_HANDLE_FILE, FS_HANDLE_DIR };

struct fs_handle {
	char	*path;
	off_t	oldoffset; /* files only */
	enum 	fs_handle_type type;
	int	fd;
	/*
	 * The sequence number field here has three states: 0 and 1
	 * indicate the sequence number we last received from
	 * the client, and 0xFF means that we haven't yet received
	 * any requests from the client and will let it decide which
	 * sequence number to start with.
	 *
	 * (The previous code here expected zero for the first
	 * request, but not everything follows that reliably.)
	 */
	uint8_t	sequence; /* also only for files */
    bool is_owner;
    bool can_read;
    bool can_write;
    bool is_locked;
    bool did_create;
    uint8_t read_only;
};

struct fs_dir_cache {
	char *path; /* Path for which this is a cache. */
	int start; /* position in the directory this list starts at */
	FTS *ftsp; /* Pass to fts_close to free f */
	FTSENT *f; /* Result of fts_children on path */
};

extern enum fs_info_format { FS_INFO_RISCOS, FS_INFO_SJ } default_infoformat;
extern bool default_safehandles;

struct fs_client {
	LIST_ENTRY(fs_client) link;
	struct aun_srcaddr host;
	int nhandles;
	struct fs_handle **handles; /* array of handles for this client */
	char *login;
	int priv;
	struct fs_dir_cache dir_cache;
	enum fs_info_format infoformat;
	bool safehandles;
};

LIST_HEAD(fs_client_head, fs_client);
extern struct fs_client_head fs_clients;

extern char discname[];
extern char *root;
extern char *fixedurd;
extern char *pwfile;
extern char *lib;
extern int default_opt4;
extern int default_fsstation;

typedef void fs_func_impl(struct fs_context *);
extern fs_func_impl fs_cli;
extern fs_func_impl fs_examine;
extern fs_func_impl fs_open;
extern fs_func_impl fs_close;
extern fs_func_impl fs_cat_header;
extern fs_func_impl fs_getbyte;
extern fs_func_impl fs_putbyte;
extern fs_func_impl fs_getbytes;
extern fs_func_impl fs_putbytes;
extern fs_func_impl fs_load;
extern fs_func_impl fs_save;
extern fs_func_impl fs_get_args;
extern fs_func_impl fs_set_args;
extern fs_func_impl fs_get_discs;
extern fs_func_impl fs_get_info;
extern fs_func_impl fs_set_info;
extern fs_func_impl fs_get_uenv;
extern fs_func_impl fs_get_eof;
extern fs_func_impl fs_get_users_on;
extern fs_func_impl fs_get_user;
extern fs_func_impl fs_get_time;
extern fs_func_impl fs_set_opt4;
extern fs_func_impl fs_logoff;
extern fs_func_impl fs_delete;
extern fs_func_impl fs_get_version;
extern fs_func_impl fs_get_disc_free;
extern fs_func_impl fs_cdirn;
extern fs_func_impl fs_create;
extern fs_func_impl fs_get_user_free;

extern void fs_set_user_free(struct fs_context *,char *);

extern void fs_unrec(struct fs_context *);
extern char *fs_cli_getarg(char **);
extern void fs_long_info(struct fs_context *, char *, FTSENT *);
extern void fs_reply(struct fs_context *, struct ec_fs_reply *, size_t);
extern void fs_cdir1(struct fs_context *, char *);
extern void fs_delete1(struct fs_context *, char *);

extern void fs_errno(struct fs_context *);
extern void fs_err(struct fs_context *, uint8_t);
extern void fs_error(struct fs_context *, uint8_t, const char *);

extern void fs_check_handles(struct fs_context *);
extern int fs_check_handle(struct fs_client *, int);
extern int fs_open_handle(struct fs_client *, char *, int, bool);
extern void fs_close_handle(struct fs_client *, int);

extern struct fs_client *fs_new_client(struct aun_srcaddr *);
extern void fs_delete_client(struct fs_client *);
extern struct fs_client *fs_find_client(struct aun_srcaddr *);

extern char *strpad(char *, int, size_t);
extern uint8_t fs_mode_to_type(mode_t);
extern uint8_t fs_mode_to_access(mode_t);
extern mode_t fs_access_to_mode(unsigned char, int);
extern char *fs_access_to_string(char *, uint8_t);
extern uint64_t fs_read_val(uint8_t *, size_t);
extern void fs_write_val(uint8_t *, uint64_t, size_t);
extern uint64_t fs_riscos_date(time_t, unsigned);
extern void fs_get_meta(FTSENT *, struct ec_fs_meta *);
extern bool fs_set_meta(FTSENT *, struct ec_fs_meta *);
extern void fs_del_meta(FTSENT *);
extern int fs_get_sin(FTSENT *);
extern time_t fs_get_birthtime(FTSENT *);
extern void fs_write_date(struct ec_fs_date *, time_t);
extern int fs_stat(const char *, struct stat *);
extern const char *fs_leafname(const char *);
extern bool fs_is_owner(struct fs_context *c,char *path);
extern bool fs_read_access(struct fs_context *c,char *path);
extern bool fs_is_user(char *);

extern char *fs_acornify_name(char *);
extern bool fs_hidden_name(char *);
extern char *fs_unixify_path(struct fs_context *, char *);

extern int fs_guess_type(FTSENT *);
extern int fs_add_typemap_name(const char *, int);
extern int fs_add_typemap_mode(mode_t, mode_t, int);
extern int fs_add_typemap_default(int);

struct user_funcs {
	char *(*validate)(char *, char const *, int *);
	char *(*urd)(char const *);
	int (*change)(char const *, char const *, char const *);
	int (*set_opt4)(char const *, int);
	int (*set_priv)(struct fs_client *, char const *, char const *);
  	int (*get_priv)(char const *);
    int (*add_user)(char *);
    bool (*is_user)(char *);
    int (*del_user)(char *);
};

extern struct user_funcs const *userfuncs;
extern struct user_funcs const user_pw;
extern struct user_funcs const user_null;

#endif
