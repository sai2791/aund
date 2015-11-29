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


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdint.h>

#include "aun.h"

/*
 * Opaque structure holding a source address.
 */
struct aun_srcaddr {
	uint8_t bytes[4];
};

extern void print_status(struct aun_packet *, ssize_t, struct aun_srcaddr *);
extern void print_job(struct aun_packet *, ssize_t, struct aun_srcaddr *);
extern void conf_init(const char *);
extern void fs_init(void);
extern void file_server(struct aun_packet *, ssize_t, struct aun_srcaddr *);

extern int debug;
extern int using_syslog;
extern char *beebem_cfg_file;
extern int beebem_ingress;
extern int default_timeout;

struct aun_funcs {
	int max_block;
	void (*setup)(void);
	struct aun_packet *(*recv)(ssize_t *outsize,
	    struct aun_srcaddr *from, int want_port);
	ssize_t (*xmit)(struct aun_packet *pkt,
			size_t len, struct aun_srcaddr *to);
	char *(*ntoa)(struct aun_srcaddr *addr);
	void (*get_stn)(struct aun_srcaddr *addr, uint8_t *out);
};

extern const struct aun_funcs *aunfuncs;
