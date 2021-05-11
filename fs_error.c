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
 * fs_error.c -- generating errors
 */

#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"
#include "fileserver.h"
#include "fs_proto.h"
#include "fs_errors.h"

const static struct {
	int errnoval;
	uint8_t fs_err;
} errnotab[] = {
	{ EPERM,    EC_FS_E_NOPRIV },
	{ ENOENT,	EC_FS_E_NOTFOUND },
	{ EIO,		EC_FS_E_DISCERR },
	{ ENOMEM,	EC_FS_E_NOMEM },
	{ EACCES,	EC_FS_E_NOACCESS },
	{ EXDEV,	EC_FS_E_RENXDEV },
	{ ENOTDIR,	EC_FS_E_NOTDIR },
	{ EISDIR,	EC_FS_E_ISDIR },
	{ ENFILE,	EC_FS_E_MANYOPEN },
	{ EMFILE,	EC_FS_E_MANYOPEN },
	{ ENOSPC,	EC_FS_E_DISCFULL },
	{ EROFS,	EC_FS_E_DISCPROT },
	{ ENAMETOOLONG,	EC_FS_E_BADNAME },
	{ ENOTEMPTY,	EC_FS_E_DIRNOTEMPTY },
	{ EUSERS,	EC_FS_E_MANYUSERS },
	{ EDQUOT,	EC_FS_E_DISCFULL },
};

#define NERRNOS (sizeof(errnotab) / sizeof(errnotab[0]))

const static struct {
	uint8_t err;
	char *msg;
} errmsgtab[] = {
    {EC_FS_E_BADEXAMINE,	"Bad EXAMINE argument"},

    {EC_FS_E_OBJNOTFILE, "Object not a file"},

    {EC_FS_E_BADINFO,	"Bad INFO argument"},
    {EC_FS_E_BADARGS,	"Bad RDARGS argument"},

    {EC_FS_E_NOMEM, "Server out of memory"},

    {EC_FS_E_USERNOTON, "User not logged on"},
    {EC_FS_E_TYPENMATC, "Types don't match"},

    {EC_FS_E_RENXDEV, "Renaming across two discs"},
    {EC_FS_E_USEREXIST, "User id. already exists"},
    {EC_FS_E_PWFFULL, "Password file full"},
    {EC_FS_E_DIRFULL, "Maximum directory size reached"},
	{EC_FS_E_DIRNOTEMPTY, "Directory not empty"},
	{EC_FS_E_ISDIR, "Is a directory"},
	{EC_FS_E_MAPDISCERR, "Disc error on map read/write"},
	{EC_FS_E_OUTSIDEFILE, "Attempt to point outside a file"},
	{EC_FS_E_MANYUSERS, "Too many users"},
	{EC_FS_E_BADPW, "Bad password"},
	{EC_FS_E_NOPRIV, "Insufficient privilege"},
	{EC_FS_E_WRONGPW, "Incorrect password"},
	{EC_FS_E_BADUSER, "User not known"},
	{EC_FS_E_NOACCESS, "Insufficient access"},
	{EC_FS_E_NOTDIR, "Object not a directory"},
	{EC_FS_E_WHOAREYOU, "Who are you?"},

	{EC_FS_E_MANYOPEN, "Too many open files"},
	{EC_FS_E_RDONLY, "File not open for update"},
	{EC_FS_E_OPEN, "Already open"},
	{EC_FS_E_LOCKED, "Entry locked"},
	{EC_FS_E_DISCFULL, "Disc full"},
	{EC_FS_E_DISCERR, "Unrecoverable disc error"},
	{EC_FS_E_BADDISC, "Disc number not found"},
	{EC_FS_E_DISCPROT, "Disc protected"},
	{EC_FS_E_BADNAME, "Bad file name"},
	{EC_FS_E_BADACCESS, "Invalid access string"},

	{EC_FS_E_NOTFOUND, "Not found"},
	{EC_FS_E_CHANNEL, "Channel"},
	{EC_FS_E_EOF, "End of file"},

	{EC_FS_E_BADSTR, "Bad string"},
	{EC_FS_E_BADCMD, "Bad command"},
};

#define NMSGS (sizeof(errmsgtab)/sizeof(errmsgtab[0]))

void
fs_errno(struct fs_context *c)
{
	int i;

	for (i=0; i<NERRNOS; i++)
		if (errnotab[i].errnoval == errno) {
			fs_err(c, errnotab[i].fs_err);
			return;
		}
	fs_error(c, 0xff, strerror(errno));
}


void
fs_err(struct fs_context *c, uint8_t err)
{
	int i;

	for (i=0; i<NMSGS; i++)
		if (errmsgtab[i].err == err) {
			fs_error(c, err, errmsgtab[i].msg);
			return;
		}
	fs_error(c, err, "Internal server error");
}

void
fs_error(struct fs_context *c, uint8_t err, const char *report)
{
	struct ec_fs_reply *reply;

	if ((reply = malloc(sizeof(*reply) + strlen(report)+2)) == NULL)
		exit(2);
	reply->command_code = EC_FS_CC_DONE;
	reply->return_code = err;
	strcpy(reply->data, report);
	*strchr(reply->data, '\0') = 13;
	if (debug) printf("fs_error: 0x%x/%s\n", err, report);
	fs_reply(c, reply, sizeof(*reply) + strlen(report) + 1);
	free(reply);
}
