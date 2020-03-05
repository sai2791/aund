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

#ifndef _FS_PROTO_H
#define _FS_PROTO_H

#include <sys/types.h>

#include <stdint.h>

#include "aun.h"

/* Acorn object types */
/* These disagree with ESUG, but seem to work with RISC OS */
#define EC_FS_TYPE_NONE	0x00 /* ESUG */
#define EC_FS_TYPE_FILE	0x01
#define EC_FS_TYPE_DIR	0x02
#define EC_FS_TYPE_SOME	0xff /* ESUG */

/* Standard Acorn access flags */
#define EC_FS_ACCESS_OR	0x01 /* Public read */
#define EC_FS_ACCESS_OW	0x02 /* Public write */
#define EC_FS_ACCESS_UR	0x04 /* Owner read */
#define EC_FS_ACCESS_UW	0x08 /* Owner write */
#define EC_FS_ACCESS_L	0x10 /* Locked */
#define EC_FS_ACCESS_D	0x20 /* Directory */

/* Boot options */
#define EC_FS_OPT4_NONE	0
#define EC_FS_OPT4_LOAD	1
#define EC_FS_OPT4_RUN	2
#define EC_FS_OPT4_EXEC	3

/* Priv Modes */
#define EC_FS_PRIV_NONE  0  /* Normal User */
#define EC_FS_PRIV_FIXED 1  /* Level 4 Dunno */
#define EC_FS_PRIV_LIMIT 2  /* MDFS Cant change passwd */
#define EC_FS_PRIV_SYST  3  /* System User */

/* Directory Access rights */
#define FS_DIR_ACCESS_OWNER	0x00
#define FS_DIR_ACCESS_PUBLIC	0xff

/*
 * Structures common to several calls.
 */

struct ec_fs_date {
	uint8_t day;
	uint8_t year_month; /* Four bits each */
};

struct ec_fs_meta {
	/*
	 * Writing a struct to explain the RISC OS interpretation of
	 * this is tricky, so I won't try.
	 */
	uint8_t load_addr[4];
	uint8_t exec_addr[4];
};

struct ec_fs_req {
	struct aun_packet aun;
	uint8_t reply_port;
	uint8_t function;
	uint8_t urd;
	uint8_t csd;
	uint8_t lib;
	uint8_t data[0]; /* more */
};

struct ec_fs_reply {
	struct aun_packet aun;
	uint8_t command_code;
/* This defines what we expect the client to do next */
#define EC_FS_CC_DONE	0 /* No further action */
#define EC_FS_CC_SAVE	1
#define EC_FS_CC_LOAD	2
#define EC_FS_CC_CAT	3
#define EC_FS_CC_INFO	4
#define EC_FS_CC_UNREC	8
#define EC_FS_CC_DISCS  10
	uint8_t return_code;
#define EC_FS_RC_OK 0
	uint8_t data[0]; /* more */
};

/* Command line decoding - function code 0 */
#define EC_FS_FUNC_CLI 		0

/* Response to *SAVE */
#define EC_FS_CC_SAVE	1
struct ec_fs_reply_cli_save {
	struct ec_fs_reply std_tx;
	struct ec_fs_meta meta;
	uint8_t size[3];
	char path[0];
};

/* Response to *LOAD */
#define EC_FS_CC_LOAD	2
struct ec_fs_reply_cli_load {
	struct ec_fs_reply std_tx;
	uint8_t load_addr[4];
	uint8_t load_addr_found;
	char path[0];
};

/* Response to *I AM */
#define EC_FS_CC_LOGON	5 /* Store handles returned */
struct ec_fs_reply_logon {
	/* Command code 5, giving initial handles and boot option */
	struct ec_fs_reply std_tx;
	uint8_t urd;
	uint8_t csd;
	uint8_t lib;
	uint8_t opt4;
};

/* Response to *SDISC */
#define EC_FS_CC_SDISC	6 /* as logon */
struct ec_fs_reply_sdisc {
	/* Command code 6, notifying client of new handles */
	struct ec_fs_reply std_tx;
	uint8_t urd;
	uint8_t csd;
	uint8_t lib;
};

/* Response to *LIB or *DIR */
#define EC_FS_CC_DIR	7 /* store CSD handle */
#define EC_FS_CC_LIB	9 /* store LIB handle */
struct ec_fs_reply_dir {
	struct ec_fs_reply std_tx;
	uint8_t new_handle;
};

/* Save - function code 1 */
#define EC_FS_FUNC_SAVE		1
struct ec_fs_req_save {
	/* NB std_tx.urd is actually acknowledge port, and not a handle */
	struct ec_fs_req std_rx;
	struct ec_fs_meta meta;
	uint8_t size[3];
	char path[0]; /* CR terminated */
};
/* before data transfer */
struct ec_fs_reply_save1 {
	struct ec_fs_reply std_tx;
	uint8_t data_port;
	uint8_t block_size[2];
};
/* after data transfer */
struct ec_fs_reply_save2 {
	struct ec_fs_reply std_tx;
	uint8_t access;
	struct ec_fs_date date;
};

/* Load - function code 2 */
#define EC_FS_FUNC_LOAD		2
struct ec_fs_req_load {
	/* NB std_tx.urd is actually data port, and not a handle */
	struct ec_fs_req std_rx;
	char path[0]; /* CR terminated */
};
/* before data transfer */
struct ec_fs_reply_load1 {
	struct ec_fs_reply std_tx;
	struct ec_fs_meta meta;
	uint8_t size[3];
	uint8_t access;
	struct ec_fs_date date;
};
/* after data transfer */
struct ec_fs_reply_load2 {
	struct ec_fs_reply std_tx;
};

/* Examine - function code 3 */
#define EC_FS_FUNC_EXAMINE	3
struct ec_fs_req_examine {
	struct ec_fs_req std_rx;
	uint8_t arg;
	uint8_t start;
	uint8_t nentries;
	char path[0];
};
struct ec_fs_reply_examine {
	struct ec_fs_reply std_tx;
	uint8_t nentries;
	/*
	 * this next byte isn't in any of the specs, but awServer puts
	 * it in and RISC OS seems to expect it.
	 */
	uint8_t undef0;
	char data[0];
};
/* ARG = 0 => all information, m/c readable */
#define EC_FS_EXAMINE_ALL	0
struct ec_fs_exall {
	char name[10];
	struct ec_fs_meta meta;
	uint8_t access;
	struct ec_fs_date date;
	uint8_t sin[3];
	uint8_t size[3];
};
struct ec_fs_reply_examine_all {
	struct ec_fs_reply std_tx;
	uint8_t nentries;
	uint8_t undef0;
	struct ec_fs_exall data[0];
};
#define SIZEOF_ec_fs_reply_examine_all(N) (sizeof(struct ec_fs_reply_examine_all) + (N)*sizeof(struct ec_fs_exall))
/* ARG = 1 => all information, character string */
#define EC_FS_EXAMINE_LONGTXT	1
/* ARG = 2 => file title only */
#define EC_FS_EXAMINE_NAME	2
struct ec_fs_exname {
	/*
	 * FSUM disagrees, and says it's name followed by access.  I
	 * follow later manuals and awServer.
	 */
	uint8_t namelen; /* Always 10, apparently */
	char name[10];
};
struct ec_fs_reply_examine_name {
	struct ec_fs_reply std_tx;
	uint8_t nentries;
	uint8_t undef0; /* XXX */
	struct ec_fs_exname data[0];
};
/* ARG = 3 => access + file title, character string */
#define EC_FS_EXAMINE_SHORTTXT	3

/* Catalogue Header - code 4 */
#define EC_FS_FUNC_CAT_HEADER	4
struct ec_fs_req_cat_header {
	struct ec_fs_req std_rx;
	char path[0]; /* CR terminated */
};
struct ec_fs_reply_cat_header {
	struct ec_fs_reply std_tx;
	/*
	 * SGT: This appears to be a request which was obsolete even
	 * in the days of the BBC B. The precise format is not
	 * documented anywhere I could find, but I ran the BBC BASIC
	 * Econet Level 1 file server, sent it this request by a
	 * hand-coded OSWORD &14, and network-sniffed it.
	 */
	char dir_name[10];	       /* space-padded */
	char space[1];		       /* just a space */
	char ownership[1];	       /* 'O'(wner) or 'P'(ublic) */
	char spaces[3];		       /* spaces */
	char csd_discname[12];	       /* zero-padded */
	char cr80[2];		       /* cr, 0x80 */
};

/* Load as command - code 5 */
#define EC_FS_FUNC_LOAD_COMMAND	5
/* As load */

/* Find (OPEN) - code 6 */
#define EC_FS_FUNC_OPEN		6
struct ec_fs_req_open {
	struct ec_fs_req std_rx;
	uint8_t must_exist; /* 0 or 1 */
	uint8_t read_only;
	char path[0];
};
struct ec_fs_reply_open {
	struct ec_fs_reply std_tx;
	uint8_t handle;
};

/* Shut (CLOSE) - code 7 */
#define EC_FS_FUNC_CLOSE	7
struct ec_fs_req_close {
	struct ec_fs_req std_rx;
	uint8_t handle;
};

/* Get byte - code 8 */
#define EC_FS_FUNC_GETBYTE	8
struct ec_fs_req_getbyte {
	struct aun_packet pkt;
	uint8_t reply_port;
	uint8_t function;
	/* Note incomplete header */
	uint8_t handle;
};
struct ec_fs_reply_getbyte {
	struct ec_fs_reply std_tx;
	uint8_t byte;
	uint8_t flag;
#define EC_FS_FLAG_LAST 0x80
#define EC_FS_FLAG_EOF 0xc0
};

/* Put byte - code 9 */
#define EC_FS_FUNC_PUTBYTE	9
struct ec_fs_req_putbyte {
	struct aun_packet pkt;
	uint8_t reply_port;
	uint8_t function;
	/* NB incomplete header */
	uint8_t handle;
	uint8_t byte;
};

/* Get bytes and put bytes - codes 10 and 11 */
#define EC_FS_FUNC_GETBYTES	10
struct ec_fs_req_getbytes {
	/* std_tx.urd is actually the reply port */
	struct ec_fs_req std_rx; /* Whatever the manual says */
	uint8_t handle;
	uint8_t use_ptr;
	uint8_t nbytes[3];
	uint8_t offset[3]; /* only if use_ptr != 0 */
};
struct ec_fs_reply_getbytes2 {
	struct ec_fs_reply std_tx;
	uint8_t flag;
	uint8_t nbytes[3];
};
#define EC_FS_FUNC_PUTBYTES	11
struct ec_fs_req_putbytes {
	struct ec_fs_req std_rx; /* Whatever the manual says */
	uint8_t handle;
	uint8_t use_ptr;
	uint8_t nbytes[3];
	uint8_t offset[3]; /* only if use_ptr != 0 */
};
struct ec_fs_reply_putbytes1 {
	struct ec_fs_reply std_tx;
	uint8_t data_port;
	uint8_t block_size[2];
};
struct ec_fs_reply_putbytes2 {
	struct ec_fs_reply std_tx;
	uint8_t zero;
	uint8_t nbytes[3];
};

/* Read random access info - code 12 */
#define EC_FS_FUNC_GET_ARGS	12
#define EC_FS_ARG_PTR   0
#define EC_FS_ARG_EXT   1
#define EC_FS_ARG_SIZE  2
struct ec_fs_req_get_args {
	struct ec_fs_req std_rx;
	uint8_t handle;
	uint8_t arg;
};
struct ec_fs_reply_get_args {
	struct ec_fs_reply std_tx;
	uint8_t val[3];
};

/* Set random access info - code 13 */
#define EC_FS_FUNC_SET_ARGS	13
struct ec_fs_req_set_args {
	struct ec_fs_req std_rx;
	uint8_t handle;
	uint8_t arg;
	uint8_t val[3];
};
/* Read disc info - code 14 */
#define EC_FS_FUNC_GET_DISCS	14
struct ec_fs_req_get_discs {
	struct ec_fs_req std_rx;
	uint8_t sdrive;
	uint8_t ndrives;
};
struct ec_fs_disc {
	uint8_t num;
	char name[16];
};
struct ec_fs_reply_get_discs {
	struct ec_fs_reply std_tx;
	uint8_t ndrives;
	struct ec_fs_disc drives[0];
};
#define SIZEOF_ec_fs_reply_discs(N) \
    (sizeof(struct ec_fs_reply_get_discs) + (N)*sizeof(struct ec_fs_disc))

/* Read logged on users - code 15 */
#define EC_FS_FUNC_GET_USERS_ON	15
struct ec_fs_req_get_users_on {
	struct ec_fs_req std_rx;
	uint8_t start;
	uint8_t nusers;
};
struct ec_fs_user_on {
	uint8_t station[2];
	char user[10];
	uint8_t priv;
};
struct ec_fs_reply_get_users_on {
	struct ec_fs_reply std_tx;
	uint8_t nusers;
	struct ec_fs_user_on users[0];
};

/* Read data and time - code 16 */
#define EC_FS_FUNC_GET_TIME	16
struct ec_fs_reply_get_time {
	struct ec_fs_reply std_tx;
	struct ec_fs_date date;
	uint8_t hours;
	uint8_t mins;
	uint8_t secs;
};

/* Read "end of file" status - code 17 */
#define EC_FS_FUNC_GET_EOF	17
struct ec_fs_req_get_eof {
	struct ec_fs_req std_rx;
	uint8_t handle;
};
struct ec_fs_reply_get_eof {
	struct ec_fs_reply std_tx;
	uint8_t status; /* 0xff if outside file, 0x00 otherwise */
};

/* Read object info - code 18 */
#define EC_FS_FUNC_GET_INFO	18
struct ec_fs_req_get_info {
	struct ec_fs_req std_rx;
	uint8_t arg;
	char path[0];
};
#define EC_FS_GET_INFO_CTIME	1
struct ec_fs_reply_info_ctime {
	struct ec_fs_reply std_tx;
	uint8_t type;
	struct ec_fs_date date;
};
#define EC_FS_GET_INFO_META		2
struct ec_fs_reply_info_meta {
	struct ec_fs_reply std_tx;
	uint8_t type;
	struct ec_fs_meta meta;
};
#define EC_FS_GET_INFO_SIZE		3
struct ec_fs_reply_info_size {
	struct ec_fs_reply std_tx;
	uint8_t type;
	uint8_t size[3]; /* little-endian */
};
#define EC_FS_GET_INFO_ACCESS      	4
struct ec_fs_reply_info_access {
	struct ec_fs_reply std_tx;
	uint8_t type;
	uint8_t access;
};
#define EC_FS_GET_INFO_ALL		5
struct ec_fs_reply_info_all {
	struct ec_fs_reply std_tx;
	uint8_t type;
	struct ec_fs_meta meta;
	uint8_t size[3];
	uint8_t access;
	struct ec_fs_date date;
};
#define EC_FS_GET_INFO_DIR	6
struct ec_fs_reply_info_dir {
	struct ec_fs_reply std_tx;
	uint8_t undef0;
	uint8_t zero;
	uint8_t ten; /* these two used by Beeb NFS apparently */
	char dir_name[10];
	uint8_t dir_access;
//#define FS_DIR_ACCESS_OWNER	0x00
//#define FS_DIR_ACCESS_PUBLIC	0xff
	uint8_t cycle; /* changes whenever dir changes */
};
#define EC_FS_GET_INFO_UID	7
struct ec_fs_reply_info_uid {
	struct ec_fs_reply std_tx;
	uint8_t type;
	uint8_t sin[3];
	uint8_t disc;
	uint8_t fsnum[2];
};

/* Set file info - code 19 */
#define EC_FS_FUNC_SET_INFO	19
struct ec_fs_req_set_info {
	struct ec_fs_req std_rx;
	uint8_t arg;
	uint8_t data[0];
};
#define EC_FS_SET_INFO_ALL	1
struct ec_fs_req_set_info_all {
	struct ec_fs_req_set_info std_si;
	struct ec_fs_meta meta;
	uint8_t access;
	char path[0];
};
#define EC_FS_SET_INFO_LOAD	2
struct ec_fs_req_set_info_load {
	struct ec_fs_req_set_info std_si;
	uint8_t load_addr[4];
	char path[0];
};
#define EC_FS_SET_INFO_EXEC	3
struct ec_fs_req_set_info_exec {
	struct ec_fs_req_set_info std_si;
	uint8_t exec_addr[4];
	char path[0];
};
#define EC_FS_SET_INFO_ACCESS	4
struct ec_fs_req_set_info_access {
	struct ec_fs_req_set_info std_si;
	uint8_t access;
	char path[0];
};

/* Delete object - code 20 */
#define EC_FS_FUNC_DELETE	20
struct ec_fs_req_delete {
	struct ec_fs_req std_rx;
	char path[0];
};
struct ec_fs_reply_delete {
	struct ec_fs_reply std_tx;
	struct ec_fs_meta meta;
	uint8_t size[3];
};

/* Read user environment - code 21 */
#define EC_FS_FUNC_GET_UENV	21
struct ec_fs_reply_get_uenv {
	struct ec_fs_reply std_tx;
	uint8_t discnamelen; 	/* Disc name length max */
	char csd_discname[16]; 	/* Name of disc with CSD on */
	char csd_leafname[10];	/* Leaf name of CSD */
	char lib_leafname[10];	/* Leaf name of lib */
};

/* Set user option - code 22 */
#define EC_FS_FUNC_SET_OPT4	22
struct ec_fs_req_set_opt4 {
	struct ec_fs_req std_rx;
	uint8_t opt4;
};

/* Log-off - code 23 */
#define EC_FS_FUNC_LOGOFF	23

/* Read user info - code 24 */
#define EC_FS_FUNC_GET_USER	24
struct ec_fs_req_get_user {
	struct ec_fs_req std_rx;
	char user[0];
};
struct ec_fs_reply_get_user {
	struct ec_fs_reply std_tx;
	uint8_t priv;
	uint8_t station[2];
};

/* Read file server version number - code 25 */
#define EC_FS_FUNC_GET_VERSION	25
struct ec_fs_reply_get_version {
	struct ec_fs_reply std_tx;
	char version[0];
};

/* Read disc free space - code 26 */
#define EC_FS_FUNC_GET_DISC_FREE	26
struct ec_fs_req_get_disc_free {
	struct ec_fs_req std_rx;
	char discname[0];
};
struct ec_fs_reply_get_disc_free {
	struct ec_fs_reply std_tx;
	uint8_t free_blocks[3]; /* Units of 0x100 bytes */
	uint8_t total_blocks[3]; /* ditto */
};

/* Create directory, specifying size - code 27 */
#define EC_FS_FUNC_CDIRN	27
struct ec_fs_req_cdirn {
	struct ec_fs_req std_rx;
	uint8_t size; /* Maximum number of sectors */
	char path[0];
};

/* Set fileserver date and time - code 28 */
#define EC_FS_FUNC_SET_TIME	28
struct ec_fs_req_set_time {
	struct ec_fs_req std_rx;
	struct ec_fs_date date;
	uint8_t hours;
	uint8_t mins;
	uint8_t secs;
};

/* Create file - code 29 */
#define EC_FS_FUNC_CREATE	29
struct ec_fs_req_create { /* almost identical to save */
	struct ec_fs_req std_rx;
	struct ec_fs_meta meta;
	uint8_t size[3];
	char path[0]; /* CR terminated */
};
struct ec_fs_reply_create { /* identical to save2 */
	struct ec_fs_reply std_tx;
	uint8_t access;
	struct ec_fs_date date;
};

/* Read user free space - code 30 */
#define EC_FS_FUNC_GET_USER_FREE 30
struct ec_fs_req_get_user_free {
	struct ec_fs_req std_rx;
	char username[0];
};
struct ec_fs_reply_get_user_free {
	struct ec_fs_reply std_tx;
	uint8_t free_bytes[4];
};
#define EC_FS_FUNC_SET_USER_FREE 31
#define EC_FS_FUNC_WHO_AM_I	32
#define EC_FS_FUNC_USERS_EXT	33
#define EC_FS_FUNC_USER_INFO_EXT 34
#define EC_FS_FUNC_COPY_DATA	35

#endif
