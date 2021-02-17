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
 * fs_misc.c - miscellaneous file server calls
 */

#include <fts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "aun.h"
#include "fs_proto.h"
#include "fs_errors.h"
#include "extern.h"
#include "fileserver.h"
#include "version.h"

void
fs_get_discs(struct fs_context *c)
{
	struct ec_fs_reply_get_discs *reply;
	struct ec_fs_req_get_discs *request =
	    (struct ec_fs_req_get_discs *)(c->req);
	int nfound;

	if (debug) printf("get discs [%d/%d]\n",
	    request->sdrive, request->ndrives);
	if (request->sdrive == 0 && request->ndrives > 0)
		nfound = 1;
	else
		nfound = 0;
	if ((reply = malloc(SIZEOF_ec_fs_reply_discs(nfound))) == NULL) exit(2);
	reply->std_tx.command_code = EC_FS_CC_DISCS;
	reply->std_tx.return_code = EC_FS_RC_OK;
	reply->ndrives = nfound;
	if (nfound) {
		reply->drives[0].num = 0;
		strncpy(reply->drives[0].name, discname,
		    sizeof(reply->drives[0].name));
		strpad(reply->drives[0].name, ' ',
		    sizeof(reply->drives[0].name));
	}
	fs_reply(c, &(reply->std_tx), SIZEOF_ec_fs_reply_discs(nfound));
	free(reply);
}

void
fs_get_info(struct fs_context *c)
{
	char *upath, *path_argv[2];
	struct ec_fs_req_get_info *request;
	FTS *ftsp;
	FTSENT *f;
	bool match; /* string compare for path and urd */

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	request = (struct ec_fs_req_get_info *)c->req;
	request->path[strcspn(request->path, "\r")] = '\0';
	if (debug) printf("get info [%d, '%s']\n", request->arg, request->path);
	upath = fs_unixify_path(c, request->path); /* This must be freed */
	if (upath == NULL) return;
	errno = 0;
	path_argv[0] = upath;
	path_argv[1] = NULL;
	ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
	f = fts_read(ftsp);
	switch (request->arg) {
	case EC_FS_GET_INFO_ACCESS: {
		struct ec_fs_reply_info_access reply;
		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			reply.access = fs_mode_to_access(f->fts_statp->st_mode);
		}
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_ALL: {
		struct ec_fs_reply_info_all reply;

		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
			memset(&(reply.meta), 0, sizeof(reply.meta));
			memset(&(reply.size), 0, sizeof(reply.size));
			memset(&(reply.access), 0, sizeof(reply.access));
			memset(&(reply.date), 0, sizeof(reply.date));
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			fs_get_meta(f, &(reply.meta));
			fs_write_val(reply.size, f->fts_statp->st_size,
			    sizeof(reply.size));
			reply.access = fs_mode_to_access(f->fts_statp->st_mode);
			fs_write_date(&(reply.date), fs_get_birthtime(f));
		}
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_CTIME: {
		struct ec_fs_reply_info_ctime reply;

		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
			memset(&(reply.date), 0, sizeof(reply.date));
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			fs_write_date(&(reply.date), fs_get_birthtime(f));
		}
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_META: {
		struct ec_fs_reply_info_meta reply;

		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
			memset(&(reply.meta), 0, sizeof(reply.meta));
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			fs_get_meta(f, &(reply.meta));
		}
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_SIZE: {
		struct ec_fs_reply_info_size reply;

		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
			memset(&(reply.size), 0, sizeof(reply.size));
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			fs_write_val(reply.size,
			    f->fts_statp->st_size, sizeof(reply.size));
		}
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_DIR:
	{
		struct ec_fs_reply_info_dir reply;

		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			fs_errno(c);
			fts_close(ftsp);
			return;
		}
		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		reply.undef0 = 0;
		reply.zero = 0;
		reply.ten = 10;
		fs_acornify_name(f->fts_name);
		if (f->fts_name[0] == '\0') strcpy(f->fts_name, "$");
		strncpy(reply.dir_name, f->fts_name, sizeof(reply.dir_name));
		strpad(reply.dir_name, ' ', sizeof(reply.dir_name));
		/* We now check ownership. See also fs_cat_header */
		/* if the path matches our urd then assume that we are the owner
                   and if not, then check if the user has priv, because they own 
		   everything.  */
		match = fs_is_owner(c, upath);
		if (match == true) {
			reply.dir_access = FS_DIR_ACCESS_OWNER;
		} else {
			reply.dir_access = FS_DIR_ACCESS_PUBLIC;
		} 

                if (c->client->priv == EC_FS_PRIV_SYST)
		{
		    reply.dir_access = FS_DIR_ACCESS_OWNER;
		} 
		    /* Even better would be to check the user and group ID, and see if
                       the directory is owned by the user but we have not implemented
		       user identifiers or group identifiers yet.  */
		reply.cycle = 0; /* XXX should fake */
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
	break;
	case EC_FS_GET_INFO_UID:
	{
		struct ec_fs_reply_info_uid reply;

		reply.std_tx.return_code = EC_FS_RC_OK;
		reply.std_tx.command_code = EC_FS_CC_DONE;
		if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
			reply.type = EC_FS_TYPE_NONE;
			memset(&(reply.sin), 0, sizeof(reply.sin));
			memset(&(reply.fsnum), 0, sizeof(reply.fsnum));
		} else {
			reply.type = fs_mode_to_type(f->fts_statp->st_mode);
			fs_write_val(reply.sin, fs_get_sin(f),
			    sizeof(reply.sin));
			reply.disc = 0;
			fs_write_val(reply.fsnum, f->fts_statp->st_dev,
			    sizeof(reply.fsnum));
			fs_reply(c, &(reply.std_tx), sizeof(reply));
		}
	}
	break;
	default:
		fs_err(c, EC_FS_E_BADINFO);
	}
	fts_close(ftsp);
	free(upath);
}

void
fs_set_info(struct fs_context *c)
{
	char *path, *upath, *path_argv[2];
	struct ec_fs_req_set_info *request;
	struct ec_fs_reply reply;
	struct ec_fs_meta meta_in, meta_out;
	uint8_t access;
	int set_load = 0, set_exec = 0, set_access = 0;
	FTS *ftsp;
	FTSENT *f;

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	request = (struct ec_fs_req_set_info *)c->req;
	if (debug) printf("set info [%d, ", request->arg);
	switch (request->arg) {
	case EC_FS_SET_INFO_ALL: {
		struct ec_fs_req_set_info_all *req2 =
			(struct ec_fs_req_set_info_all *)request;
		memcpy(&meta_in, &(req2->meta), sizeof(meta_in));
		access = req2->access;
		path = req2->path;
		set_load = set_exec = 1;
		break;
	}
	case EC_FS_SET_INFO_LOAD: {
		struct ec_fs_req_set_info_load *req2 =
			(struct ec_fs_req_set_info_load *)request;
		memcpy(&meta_in.load_addr, &(req2->load_addr),
		       sizeof(meta_in.load_addr));
		path = req2->path;
		set_load = 1;
		break;
	}
	case EC_FS_SET_INFO_EXEC: {
		struct ec_fs_req_set_info_exec *req2 =
			(struct ec_fs_req_set_info_exec *)request;
		memcpy(&meta_in.exec_addr, &(req2->exec_addr),
		       sizeof(meta_in.exec_addr));
		path = req2->path;
		set_exec = 1;
		break;
	}
	case EC_FS_SET_INFO_ACCESS: {
		struct ec_fs_req_set_info_access *req2 =
			(struct ec_fs_req_set_info_access *)request;
		access = req2->access;
		path = req2->path;
		set_access = 1;
		break;
	}
	default:
		if (debug) printf("]\n");
		fs_err(c, EC_FS_E_BADINFO);
		return;
	}

	if (debug) {
		if (set_load)
			printf("%02x%02x%02x%02x, ",
			    meta_in.load_addr[0], meta_in.load_addr[1],
			    meta_in.load_addr[2], meta_in.load_addr[3]);
		if (set_exec)
			printf("%02x%02x%02x%02x, ",
			    meta_in.exec_addr[0], meta_in.exec_addr[1],
			    meta_in.exec_addr[2], meta_in.exec_addr[3]);
		if (set_access)
			printf("%02x, ", access);
	}
	path[strcspn(path, "\r")] = '\0';
	if (debug) printf("%s]\n", path);

	upath = fs_unixify_path(c, path); /* This must be freed */
	if (upath == NULL) return;
	errno = 0;
	path_argv[0] = upath;
	path_argv[1] = NULL;
	ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
	f = fts_read(ftsp);
	if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
		fs_errno(c);
		goto out;
	}
	if (set_load || set_exec) {
		fs_get_meta(f, &meta_out);
		if (set_load)
			memcpy(meta_out.load_addr, meta_in.load_addr,
			       sizeof(meta_in.load_addr));
		if (set_exec)
			memcpy(meta_out.exec_addr, meta_in.exec_addr,
			       sizeof(meta_in.exec_addr));
		if (!fs_set_meta(f, &meta_out)) {
			fs_errno(c);
			goto out;
		}
	}
	/*
	 * We don't try to set the access on directories.  Acorn file
	 * servers historically didn't support permissions on
	 * directories, and NetFS and the Filer both do some rather
	 * strange things with them.
	 */
	if (set_access && !S_ISDIR(f->fts_statp->st_mode)) {
		/* XXX Should chose usergroup sensibly */
		if (chmod(f->fts_accpath, fs_access_to_mode(access, 0)) != 0) {
			fs_errno(c);
			goto out;
		}
	}
	reply.return_code = EC_FS_RC_OK;
	reply.command_code = EC_FS_CC_DONE;
	fs_reply(c, &reply, sizeof(reply));
out:
	fts_close(ftsp);
	free(upath);
}

void
fs_get_uenv(struct fs_context *c)
{
	struct ec_fs_reply_get_uenv reply;
	char tmp[11];

	if (debug) printf("get user environment\n");
	reply.std_tx.command_code = EC_FS_CC_DONE;
	reply.std_tx.return_code = EC_FS_RC_OK;
	reply.discnamelen = sizeof(reply.csd_discname);
	strncpy(reply.csd_discname, discname, sizeof(reply.csd_discname));
	strpad(reply.csd_discname, ' ', sizeof(reply.csd_discname));
	tmp[10] = '\0';
	if (c->req->csd) {
		strncpy(tmp,
		    fs_leafname(c->client->handles[c->req->csd]->path),
			sizeof(tmp) - 1);
		fs_acornify_name(tmp);
		if (tmp[0] == '\0') strcpy(tmp, "$");
	}
	else
		tmp[0] = '\0';
	strncpy(reply.csd_leafname, tmp, sizeof(reply.csd_leafname));
	strpad(reply.csd_leafname, ' ', sizeof(reply.csd_leafname));
	if (c->req->lib) {
		strncpy(tmp,
		    fs_leafname(c->client->handles[c->req->lib]->path),
			sizeof(tmp) - 1);
		fs_acornify_name(tmp);
		if (tmp[0] == '\0') strcpy(tmp, "$");
	}
	else
		tmp[0] = '\0';
	strncpy(reply.lib_leafname, tmp, sizeof(reply.lib_leafname));
	strpad(reply.lib_leafname, ' ', sizeof(reply.lib_leafname));
	fs_reply(c, &(reply.std_tx), sizeof(reply));
}

void
fs_cat_header(struct fs_context *c)
{
	struct ec_fs_req_cat_header *request;
	struct ec_fs_reply_cat_header reply;
	char *upath, *path_argv[2];
	char *oururd;
	bool match;	
	FTS *ftsp;
	FTSENT *f;

	request = (struct ec_fs_req_cat_header *)c->req;
	request->path[strcspn(request->path, "\r")] = '\0'; 
	if (debug) printf("catalogue header [%s]\n", request->path);
	upath = fs_unixify_path(c, request->path); /* This must be freed */
	if (upath == NULL) return;
	errno = 0;
	path_argv[0] = upath;
	path_argv[1] = NULL;
	ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
	f = fts_read(ftsp);
	if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
		fs_errno(c);
		fts_close(ftsp);
		return;
	}

	reply.std_tx.return_code = EC_FS_RC_OK;
	reply.std_tx.command_code = EC_FS_CC_DONE;

	strncpy(reply.csd_discname, discname, sizeof(reply.csd_discname));
	strpad(reply.csd_discname, '\0', sizeof(reply.csd_discname));

	fs_acornify_name(f->fts_name);
	if (f->fts_name[0] == '\0') strcpy(f->fts_name, "$");
	strncpy(reply.dir_name, f->fts_name, sizeof(reply.dir_name));
	strpad(reply.dir_name, ' ', sizeof(reply.dir_name));

	/* We now check ownership. See also EC_FS_GET_INFO_DIR */
	/*
  	   Need to implement this check here, if we are in or below the users URD then assume
	   that they are the owner (kludge for now).
        */
	oururd = userfuncs->urd(c->client->login);
	fs_acornify_name(oururd);
	printf("/g users [%s], URD [%s]\n", c->client->login,oururd);

    match = fs_is_owner(c, upath);
	if (match == true) {
	        reply.ownership[0] = 'O';
	} else {
	        reply.ownership[0] = 'P';
	}

 	if (c->client->priv == EC_FS_PRIV_SYST)
	{
	    reply.ownership[0] = 'O';
	} 

	memset(reply.space, ' ', sizeof(reply.space));
	memset(reply.spaces, ' ', sizeof(reply.spaces));
	memcpy(reply.cr80, "\r\x80", sizeof(reply.cr80));
	fs_reply(c, &(reply.std_tx), sizeof(reply));

	fts_close(ftsp);
	free(upath);
}

void
fs_logoff(struct fs_context *c)
{
	struct ec_fs_reply reply;

	if (debug) printf ("log off\n");
	if (c->client != NULL)
		fs_delete_client(c->client);
	reply.command_code = EC_FS_CC_DONE;
	reply.return_code = EC_FS_RC_OK;
	fs_reply(c, &reply, sizeof(reply));
}

void
fs_get_users_on(struct fs_context *c)
{
	struct ec_fs_reply_get_users_on *reply;
	struct ec_fs_req_get_users_on *request;
	struct fs_client *ent;
	uint8_t *p;
	int i;

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	request = (struct ec_fs_req_get_users_on *)(c->req);
	if (debug)
		printf("users on [%d/%d]\n", request->start, request->nusers);
	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	reply = malloc(sizeof(*reply) + (request->nusers * (2+11+1)));
	if (reply == NULL) {
		fs_err(c, EC_FS_E_NOMEM);
		return;
	}
	ent = fs_clients.lh_first;
	for (i = 0; i < request->start && ent != NULL;
	     ent = ent->link.le_next) {
		i++;
	}
	p = (uint8_t *)reply->users;
	for (i = 0; i < request->nusers && ent != NULL;
	     ent = ent->link.le_next) {
		/*
		 * The Econet System User Guide, and fs_proto.h, say
		 * that this function returns a sequence of 13-byte
		 * records consisting of station number (2 bytes),
		 * space-padded username (10 bytes) and privilege
		 * byte. However, the RISC OS PRM says it returns a
		 * sequence of variable-length records consisting of
		 * station number (2 bytes), CR-terminated username
		 * (up to 11 bytes) and privilege byte.
		 *
		 * My (SGT's) old software that ran on Beebs
		 * expected the latter, so I've gone with that.
		 */
		aunfuncs->get_stn(&ent->host, p);
		p += 2;
		p += sprintf(p, "%.10s\r", ent->login);
		*p++ = c->client->priv;  /* Users may now have individual priviledge flags set */
		i++;
	}
	reply->nusers = i;
	reply->std_tx.command_code = EC_FS_CC_DONE;
	reply->std_tx.return_code = EC_FS_RC_OK;
	fs_reply(c, &(reply->std_tx), p - (uint8_t *)reply);
	free(reply);
}

void
fs_get_user(struct fs_context *c)
{
	struct ec_fs_reply_get_user reply;
	struct ec_fs_req_get_user *request;
	struct fs_client *ent;

	request = (struct ec_fs_req_get_user *)(c->req);
	request->user[strcspn(request->user, "\r")] = '\0';
	if (debug) printf("get user info [%s]\n", request->user);
	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	for (ent = fs_clients.lh_first; ent != NULL; ent = ent->link.le_next)
		if (!strcmp(request->user, ent->login))
			break;
	if (!ent) {
		reply.std_tx.command_code = EC_FS_CC_DONE;
		reply.std_tx.return_code = EC_FS_E_USERNOTON;
		fs_reply(c, &(reply.std_tx), sizeof(reply.std_tx));
	} else {
		aunfuncs->get_stn(&ent->host, reply.station);
		reply.priv = ent->priv; /* Use priv from passwd file */
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	}
}

void
fs_delete(struct fs_context *c)
{
	struct ec_fs_req_delete *request;

	request = (struct ec_fs_req_delete *)(c->req);
	request->path[strcspn(request->path, "\r")] = '\0';
	if (debug) printf("delete [%s]\n", request->path);
	fs_delete1(c, request->path);
}

void
fs_delete1(struct fs_context *c, char *path)
{
	char *upath, *acornpath, *path_argv[2];
	FTS *ftsp;
	FTSENT *f;
    bool is_owner;

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	if ((upath = fs_unixify_path(c, path)) == NULL) return;
	acornpath = malloc(10 + strlen(upath));
	if (acornpath == NULL) {
		free(upath);
		fs_err(c, EC_FS_E_NOMEM);
		return;
	}
	sprintf(acornpath, "%s/.Acorn", upath);

    is_owner = fs_is_owner(c, upath); // Check for ownership

	path_argv[0] = upath;
	path_argv[1] = NULL;
	ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
	f = fts_read(ftsp);
    if (f->fts_statp->st_mode & S_IXUSR)
    {
        // File is locked so report error and exit
        goto nodeleteallowed;
    }
    if (is_owner)
    {
        // Check we have write access to delete
        if (!(f->fts_statp->st_mode & S_IWUSR))
        {
            goto noaccess;
        }
    } else {  // Not Owner so check if we have world write permissions
        if (!(f->fts_statp->st_mode & S_IWOTH))
        {
            goto noaccess;
        }
    }
	if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
		fs_errno(c);
		goto out;
	} else if (S_ISDIR(f->fts_statp->st_mode)) {
		rmdir(acornpath);
		if (rmdir(upath) < 0) {
			fs_errno(c);
			goto out;
		}
	} else {
		if (unlink(upath) < 0) {
			fs_errno(c);
			goto out;
		}
	}
	if (c->req->function == EC_FS_FUNC_DELETE) {
		struct ec_fs_reply_delete reply;

		/*
		 * I'm not quite sure why it's necessary to return
		 * the metadata and size of something we've just
		 * deleted, but there we go.
		 */
		fs_write_val(reply.size, f->fts_statp->st_size,
		    sizeof(reply.size));
		fs_get_meta(f, &(reply.meta));
		reply.std_tx.command_code = EC_FS_CC_DONE;
		reply.std_tx.return_code = EC_FS_RC_OK;
		fs_reply(c, &(reply.std_tx), sizeof(reply));
	} else {
		struct ec_fs_reply reply;

		reply.command_code = EC_FS_CC_DONE;
		reply.return_code = EC_FS_RC_OK;
		fs_reply(c, &reply, sizeof(reply));
	}
	fs_del_meta(f);
out:
	fts_close(ftsp);
	free(acornpath);
	free(upath);
    return;

nodeleteallowed:
    fts_close(ftsp);
    free(acornpath);
    free(upath);
    fs_err(c, EC_FS_E_LOCKED);
    return;    

noaccess:
    fts_close(ftsp);
    free(acornpath);
    free(upath);
    fs_err(c, EC_FS_E_NOACCESS);
    return;    
}

void
fs_cdirn(struct fs_context *c)
{
	struct ec_fs_req_cdirn *request;

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	request = (struct ec_fs_req_cdirn *)(c->req);
	request->path[strcspn(request->path, "\r")] = '\0';
	if (debug) printf("cdirn [%s]\n", request->path);
	fs_cdir1(c, request->path);
}

void
fs_cdir1(struct fs_context *c, char *path)
{
	struct ec_fs_reply reply;
	char *upath;
    bool is_owner = false;

	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}
	upath = fs_unixify_path(c, path);
    is_owner = fs_is_owner(c, upath);
    
    if (is_owner == false)
    {
        fs_err(c, EC_FS_E_NOACCESS);
        return;
    }

	if (upath == NULL) return;
	if (mkdir(upath, 0777) < 0) {
		fs_errno(c);
	} else {
		reply.command_code = EC_FS_CC_DONE;
		reply.return_code = EC_FS_RC_OK;
		fs_reply(c, &reply, sizeof(reply));
	}
	free(upath);
}

void
fs_set_opt4(struct fs_context *c)
{
	struct ec_fs_reply reply;
	struct ec_fs_req_set_opt4 *request;
	int opt4;

	request = (struct ec_fs_req_set_opt4 *)(c->req);
	opt4 = request->opt4 & 0xF;
	if (debug) printf(" -> set boot option [%d]\n", opt4);
	if (c->client == NULL) {
		fs_err(c, EC_FS_E_WHOAREYOU);
		return;
	}

	if (userfuncs->set_opt4(c->client->login, opt4)) {
		fs_error(c, 0xff, "Not allowed");
		return;
	}
	reply.command_code = EC_FS_CC_DONE;
	reply.return_code = EC_FS_RC_OK;
	fs_reply(c, &reply, sizeof(reply));
}

void
fs_get_time(struct fs_context *c)
{
	struct ec_fs_reply_get_time reply;
	time_t t;
	struct tm tm;

	/*
	 * SGT: I actually can't see any reason to restrict this
	 * call to logged-in users.
	 */

	if (debug) printf(" -> get time\n");

	t = time(NULL);
	fs_write_date(&(reply.date), t);
	tm = *localtime(&t);
	reply.hours = tm.tm_hour;
	reply.mins = tm.tm_min;
	reply.secs = tm.tm_sec;

	reply.std_tx.command_code = EC_FS_CC_DONE;
	reply.std_tx.return_code = EC_FS_RC_OK;
	fs_reply(c, &(reply.std_tx), sizeof(reply));
}

void
fs_get_version(struct fs_context *c)
{
	struct {
		struct ec_fs_reply_get_version reply;
		char version[40];
	} reply;

	/*
	 * SGT: I actually can't see any reason to restrict this
	 * call to logged-in users.
	 */

	if (debug) printf(" -> get version\n");

	reply.reply.std_tx.command_code = EC_FS_CC_DONE;
	reply.reply.std_tx.return_code = EC_FS_RC_OK;

	/*
	 * The RISC OS PRM says that the version string must consist
	 * of nine characters describing the file server type, then
	 * a space, then a version number of the form n.xy. It says
	 * nothing about a CR terminator.
	 *
	 * However, the Econet System User Guide does say there must
	 * be a terminating CR, and SJ Research *VERS command
	 * definitely expects one. I compromise by returning the
	 * terminating CR _in addition_ to a string matching the
	 * PRM's specification.
	 */
	sprintf(reply.version, "%-9.9s %x.%02x\r",
	    AUND_FS_DESCR, AUND_VERSION_MAJOR, AUND_VERSION_MINOR);
	fs_reply(c, &(reply.reply.std_tx), sizeof(reply.reply) + 15);
}

void
fs_get_disc_free(struct fs_context *c)
{
	struct ec_fs_reply_get_disc_free reply;
	struct ec_fs_req_get_disc_free *request;
	struct statvfs f;
	unsigned long long bfree, bytes;

	request = (struct ec_fs_req_get_disc_free *)(c->req);
	request->discname[strcspn(request->discname, "\r")] = '\0';
	if (debug) printf("get disc free [%s]", request->discname);
	/*
	 * XXX To support multiple discs, we might want to look at
	 * the disc name passed in and resolve it to a Unix path.
	 * For now, though, just assume it refers to the only disc
	 * we export.
	 */
	if (statvfs(".", &f) != 0) {
		fs_errno(c);
		return;
	}
	/* XXX Handle overflow? */
	bytes = f.f_blocks * f.f_frsize;
	bfree = f.f_bfree * f.f_frsize;
	if (bytes > 0xffffffff) bytes = 0xffffffff;
	if (bfree > 0xffffffff) bfree = 0xffffffff;
	fs_write_val(reply.free_blocks, bfree>>8, sizeof(reply.free_blocks));
	fs_write_val(reply.total_blocks, bytes>>8, sizeof(reply.total_blocks));
	reply.std_tx.command_code = EC_FS_CC_DONE;
	reply.std_tx.return_code = EC_FS_RC_OK;
	fs_reply(c, &(reply.std_tx), sizeof(reply));
}

void
fs_get_user_free(struct fs_context *c)
{
	struct ec_fs_reply_get_user_free reply;
	struct ec_fs_req_get_user_free *request;
	struct statvfs f;
	unsigned long long bavail;

	request = (struct ec_fs_req_get_user_free *)(c->req);
	request->username[strcspn(request->username, "\r")] = '\0';
	if (debug) printf("get user free [%s]", request->username);
	/*
	 * XXX In an ideal world, we might look at quotas here, but in
	 * an ideal world there'd be a standardised way of doing that.
	 */
	if (statvfs(".", &f) != 0) {
		fs_errno(c);
		return;
	}
	/* XXX Handle overflow? */
	bavail = f.f_bavail * f.f_frsize;
	if (bavail > 0xffffffff) bavail = 0xffffffff;
	fs_write_val(reply.free_bytes, bavail, sizeof(reply.free_bytes));
	reply.std_tx.command_code = EC_FS_CC_DONE;
	reply.std_tx.return_code = EC_FS_RC_OK;
	fs_reply(c, &(reply.std_tx), sizeof(reply));
}
