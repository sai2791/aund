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
#include <sys/time.h>

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "aun.h"
#include "extern.h"
#include "fileserver.h"

#define EC_PORT_FS 0x99

extern const struct aun_funcs aun, beebem;

int debug = 0;
int foreground = 0;
int using_syslog = 1;
char *beebem_cfg_file = NULL;
const struct aun_funcs *aunfuncs = &aun;
char *progname;

volatile int painful_death = 0;

static void sig_init(void);
static void sigcatcher(int);

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-dDfsS] [-c config]\n", progname);
	exit(EXIT_FAILURE);
}

static char const *curpidfile;

static void
unpidfile(void)
{

	unlink(curpidfile);
}

static void
dopidfile(char const *pidfile)
{
	FILE *f;

	if ((f = fopen(pidfile, "w")) == NULL) {
		syslog(LOG_ERR, "%s: %m", pidfile);
		return;
	}
	fprintf(f, "%d\n", getpid());
	if (fclose(f) != 0)
		syslog(LOG_ERR, "%s: %m", pidfile);
	curpidfile = pidfile;
	atexit(unpidfile);
}

int
main(int argc, char *argv[])
{
	char const *conffile = "/etc/aund.conf";
	char const *pidfile = "/var/run/aund.pid";
	int c;
	int override_debug = -1;
	int override_syslog = -1;

	progname = argv[0];
	while ((c = getopt(argc, argv, "c:dDfp:sS")) != -1) {
		switch (c) {
		case '?':
			usage();      /* getopt parsing error */
		case 'c':
			conffile = optarg;
			break;
		case 'd':
			override_debug = 1;
			break;
		case 'D':
			override_debug = 0;
			break;
		case 'f':
			foreground = 1;
			break;
		case 'p':
			pidfile = optarg;
		case 's':
			override_syslog = 1;
			break;
		case 'S':
			override_syslog = 0;
			break;
		}
	}

	sig_init();
	conf_init(conffile);
	if (beebem_cfg_file)
		aunfuncs = &beebem;

	fs_init();

	/*
	 * Override specifications from the configuration file with
	 * those from the command line.
	 */
	if (override_debug != -1)
		debug = override_debug;
	if (override_syslog != -1)
		using_syslog = override_syslog;

	if (debug) setlinebuf(stdout);

	aunfuncs->setup();

	/*
	 * We'll use relative pathnames for all our file accesses,
	 * so start by setting our cwd to the root of the fs we're
	 * serving.
	 */
	if (chdir(root) < 0)
		err(1, "%s: chdir", root);

	if (!(debug || foreground))
		if (daemon(1, 0) != 0)
			err(1, "daemon");
	if (using_syslog) {
		openlog(progname, LOG_PID | (debug ? LOG_PERROR : 0),
			LOG_DAEMON);
		syslog(LOG_NOTICE, "started");
	}
	dopidfile(pidfile);
	if (debug)
		printf("started\n");

	for (;!painful_death;) {
		ssize_t msgsize;
		struct aun_packet *pkt;
		struct aun_srcaddr from;

		memset(&from, 0, sizeof(from)); /* all hosts */
		pkt = aunfuncs->recv(&msgsize, &from, EC_PORT_FS);

		switch (pkt->dest_port) {
		case EC_PORT_FS:
			if (debug) printf("\n\t(file server: ");
			file_server(pkt, msgsize, &from);
			if (debug) printf(")");
			break;
		default:
			assert(!"Packet received from wrong port");
		}
		if (debug) printf("\n");
	}
	return 0;
}

static void
sig_init(void)
{
	struct sigaction sa;

	sa.sa_handler = sigcatcher;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
}

static void
sigcatcher(int s)
{

	painful_death = 1;
}
