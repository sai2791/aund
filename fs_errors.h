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

#ifndef _FS_ERRORS_H
#define _FS_ERRORS_H

#define EC_FS_E_BADEXAMINE	0x4f

#define EC_FS_E_BADINFO		0x8e
#define EC_FS_E_BADARGS		0x8f

#define EC_FS_E_NOMEM		0x90

#define EC_FS_E_USERNOTON	0xae

#define EC_FS_E_RENXDEV		0xb0
#define EC_FS_E_USEREXIST	0xb1
#define EC_FS_E_PWFFULL		0xb2
#define EC_FS_E_DIRFULL		0xb3
#define EC_FS_E_DIRNOTEMPTY	0xb4
#define EC_FS_E_ISDIR		0xb5
#define EC_FS_E_MAPDISCERR	0xb6
#define EC_FS_E_OUTSIDEFILE	0xb7
#define EC_FS_E_MANYUSERS	0xb8
#define EC_FS_E_BADPW		0xb9
#define EC_FS_E_NOPRIV		0xba
#define EC_FS_E_WRONGPW		0xbb
#define EC_FS_E_BADUSER		0xbc
#define EC_FS_E_NOACCESS	0xbd
#define EC_FS_E_NOTDIR		0xbe
#define EC_FS_E_WHOAREYOU	0xbf

#define EC_FS_E_MANYOPEN	0xc0
#define EC_FS_E_RDONLY		0xc1
#define EC_FS_E_OPEN		0xc2
#define EC_FS_E_LOCKED		0xc3
#define EC_FS_E_DISCFULL	0xc6
#define EC_FS_E_DISCERR		0xc7
#define EC_FS_E_BADDISC		0xc8
#define EC_FS_E_DISCPROT	0xc9
#define EC_FS_E_BADNAME		0xcc
#define EC_FS_E_BADACCESS	0xcf

#define EC_FS_E_NOTFOUND	0xd6
#define EC_FS_E_CHANNEL		0xde
#define EC_FS_E_EOF		0xdf

#define EC_FS_E_BADSTR		0xfd
#define EC_FS_E_BADCMD		0xfe

#endif
