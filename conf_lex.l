/* -*- fundamental -*- */
/*-
 * Copyright (c) 2010 Simon Tatham
 * Copyright (c) 1998, 2010 Ben Harris
 * Copyright (c) 2010 James Woodcock
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
 * conf_lex.l - lexer for configuration files.
 */

%option noyywrap
%option prefix="cfy"
%option outfile="lex.yy.c"
%{
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <string.h>
#include <assert.h>

#include "extern.h"
#include "fileserver.h"

#ifndef S_IFIFO
#define S_IFIFO 0
#endif
#ifndef S_IFCHR
#define S_IFCHR 0
#endif
#ifndef S_IFDIR
#define S_IFDIR 0
#endif
#ifndef S_IFDIR
#define S_IFDIR 0
#endif
#ifndef S_IFBLK
#define S_IFBLK 0
#endif
#ifndef S_IFREG
#define S_IFREG 0
#endif
#ifndef S_IFLNK
#define S_IFLNK 0
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0
#endif
#ifndef S_IFWHT
#define S_IFWHT 0
#endif

#define CF_EOF 0
#define CF_FUNC 1
#define CF_BOOLEAN 2
#define CF_WORD 3
#define CF_NEWLINE 4

static void conf_read_file(const char *);

union cfything {
	int boolean;
	struct {
		void (*func)(union cfything *);
		mode_t mode;
	} func;
};

static void conf_cmd_debug(union cfything *);
static void conf_cmd_syslog(union cfything *);
static void conf_cmd_root(union cfything *);
static void conf_cmd_urd(union cfything *);
static void conf_cmd_pwfile(union cfything *);
static void conf_cmd_lib(union cfything *);
static void conf_cmd_beebem(union cfything *);
static void conf_cmd_infofmt(union cfything *);
static void conf_cmd_safehandles(union cfything *);
static void conf_cmd_opt4(union cfything *);
static void conf_cmd_timeout(union cfything *);
static void conf_cmd_typemap_name(union cfything *);
static void conf_cmd_typemap_perm(union cfything *);
static void conf_cmd_typemap_type(union cfything *);
static void conf_cmd_typemap_default(union cfything *);

static void dequote(char *);
	
static int cfylex(int start, union cfything *thing);

static int cfy_line;

#define YY_DECL static int cfylex(int start, union cfything *thing)

%}


%s BORING
%s TYPEMAP
%s TYPEMAP_TYPE
%s BOOLEAN
%%

	if (start != -1) BEGIN(start);

 /* Backslash-escaped newline is completely ignored */
\\\n			cfy_line++;

 /* Newline, with optional comment before it. Ignored in INITIAL state;
  * otherwise returned as a token so that commands can detect premature
  * running-out-of-arguments. */
(#[^\n]*)?\n		cfy_line++; if (YY_START != INITIAL) { BEGIN(INITIAL); return CF_NEWLINE; }

 /* Ignore whitespace except insofar as it splits words */
[[:blank:]]+		/* do nothing */

 /* In starting state, recognise main config keywords, return them as
  * function pointers, and switch into BORING state so that subsequent
  * words will be returned as CF_WORD. */
<INITIAL>{
  typemap	BEGIN(TYPEMAP);
  debug		BEGIN(BORING); thing->func.func = conf_cmd_debug; return CF_FUNC;
  syslog	BEGIN(BORING); thing->func.func = conf_cmd_syslog; return CF_FUNC;
  root		BEGIN(BORING); thing->func.func = conf_cmd_root; return CF_FUNC;
  lib|library	BEGIN(BORING); thing->func.func = conf_cmd_lib; return CF_FUNC;
  urd		BEGIN(BORING); thing->func.func = conf_cmd_urd; return CF_FUNC;
  pwfile	BEGIN(BORING); thing->func.func = conf_cmd_pwfile; return CF_FUNC;
  opt4		BEGIN(BORING); thing->func.func = conf_cmd_opt4; return CF_FUNC;
  timeout	BEGIN(BORING); thing->func.func = conf_cmd_timeout; return CF_FUNC;
  beebem	BEGIN(BORING); thing->func.func = conf_cmd_beebem; return CF_FUNC;
  info([_-]?(fmt|format))	BEGIN(BORING); thing->func.func = conf_cmd_infofmt; return CF_FUNC;
  safe[_-]?handles	BEGIN(BORING); thing->func.func = conf_cmd_safehandles; return CF_FUNC;
}
<TYPEMAP>{
  name		BEGIN(BORING); thing->func.func = conf_cmd_typemap_name; return CF_FUNC;
  perm		BEGIN(BORING); thing->func.func = conf_cmd_typemap_perm; return CF_FUNC;
  type		BEGIN(TYPEMAP_TYPE);
  default	BEGIN(BORING); thing->func.func = conf_cmd_typemap_default; return CF_FUNC;
}
<TYPEMAP_TYPE>{
  fifo		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFIFO; return CF_FUNC;
  chr		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFCHR; return CF_FUNC;
  dir		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFDIR; return CF_FUNC;
  blk		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFBLK; return CF_FUNC;
  reg		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFREG; return CF_FUNC;
  lnk		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFLNK; return CF_FUNC;
  sock		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFSOCK; return CF_FUNC;
  wht		BEGIN(BORING); thing->func.func = conf_cmd_typemap_type; thing->func.mode = S_IFWHT; return CF_FUNC;
}
<BOOLEAN>{
  yes|on|true	*(int *)thing = 1; BEGIN(BORING); return CF_BOOLEAN;
  no|off|false	*(int *)thing = 0; BEGIN(BORING); return CF_BOOLEAN;
}

 /* Any word without a specific meaning from context is returned as CF_WORD. */
'([^'\n]|'')*'		dequote(cfytext); return CF_WORD; /* [deconfuse jed syntax highlighting: '] */
[^[:blank:]\n]+		return CF_WORD;
<<EOF>>			return CF_EOF;

%%

void
conf_init(const char *path)
{

	conf_read_file(path);
}

static void
conf_read_file(const char *path)
{
        FILE *f;
	union cfything thing;
	int tok, nl;

	f = fopen(path, "r");
	if (f == NULL) err(1, "%s", path);
	cfyin = f;
	cfy_line = 1;
	while ((tok = cfylex(-1, &thing)) != CF_EOF) {
		switch (tok) {
		    case CF_FUNC:
			/*
			 * Call the specified config subfunction,
			 * which will call cfylex again in states
			 * other than INITIAL. It won't
			 * automatically reset the state to INITIAL,
			 * though: that'll only happen if a newline
			 * is parsed. So subfunctions can safely
			 * just eat a fixed number of arguments, in
			 * which case we'll come back here still in
			 * BORING state and any further words on the
			 * same line will be treated as syntax
			 * errors; alternatively, a subfunction can
			 * keep retrieving tokens until it sees
			 * CF_NEWLINE, in which case we'll come back
			 * here in INITIAL state and be ready to
			 * parse the next directive.
			 */
			thing.func.func(&thing);
			break;
		    case CF_WORD:
			errx(1, "%s:%d: Syntax error: '%s'", path, cfy_line,
			     cfytext);
			break;
		}
	}
	fclose(f);
}

static void
dequote(char *str)
{
	/*
	 * Given a string starting and ending with a single quote,
	 * strip the quotes off, and reduce doubled quotes in the
	 * middle of it. This is applied to any single-quoted
	 * CF_WORD.
	 */
	char *p = str;
	assert(*str == '\'');
	str++;
	while (1) {
		if (*str == '\'' && str[1] == '\'') {
			*p++ = '\'';
			str += 2;
		} else if (*str && *str != '\'') {
			*p++ = *str++;
		} else
			break;
	}
	assert(*str == '\'' && !str[1]);
	*p = '\0';
}

static void
conf_cmd_debug(union cfything *xthing)
{
	union cfything thing;
	if (cfylex(BOOLEAN, &thing) != CF_BOOLEAN)
		errx(1, "no boolean for debug");
	debug = thing.boolean;
}

static void
conf_cmd_syslog(union cfything *xthing)
{
	union cfything thing;
	if (cfylex(BOOLEAN, &thing) != CF_BOOLEAN)
		errx(1, "no boolean for syslog");
	using_syslog = thing.boolean;
}

static void
conf_cmd_root(union cfything *thing)
{

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no root specified");
	root = malloc(cfyleng + 1);
	strcpy(root, cfytext);
}

static void
conf_cmd_urd(union cfything *thing)
{

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no urd specified");
	fixedurd = malloc(cfyleng + 1);
	strcpy(fixedurd, cfytext);
}

static void
conf_cmd_pwfile(union cfything *thing)
{

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no password file specified");
	pwfile = malloc(cfyleng + 1);
	strcpy(pwfile, cfytext);
}

static void
conf_cmd_lib(union cfything *thing)
{

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no library specified");
	lib = malloc(cfyleng + 1);
	strcpy(lib, cfytext);
}

static void
conf_cmd_beebem(union cfything *thing)
{
	int ret;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no beebem configuration file specified");
	beebem_cfg_file = malloc(cfyleng + 1);
	strcpy(beebem_cfg_file, cfytext);

	while ((ret = cfylex(BORING, NULL)) == CF_WORD) {
		if (!strcasecmp(cfytext, "ingress"))
			beebem_ingress = 1;
		else if (!strcasecmp(cfytext, "noingress"))
			beebem_ingress = 0;
		else
			errx(1, "unrecognised beebem option: '%s'", cfytext);
	}
}

static void
conf_cmd_infofmt(union cfything *thing)
{

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no *INFO format keyword specified");
	if (!strcasecmp(cfytext, "riscos"))
		default_infoformat = FS_INFO_RISCOS;
	else if (!strcasecmp(cfytext, "sj"))
		default_infoformat = FS_INFO_SJ;
}

static void
conf_cmd_safehandles(union cfything *xthing)
{
	union cfything thing;
	if (cfylex(BOOLEAN, &thing) != CF_BOOLEAN)
		errx(1, "no boolean for safehandles");
	default_safehandles = thing.boolean;
}

static void
conf_cmd_opt4(union cfything *thing)
{
	char *endptr;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no boot option specified");
	default_opt4 = strtol(cfytext, &endptr, 0);
	if (*endptr != '\0')
		errx(1, "bad boot option");
}

static void
conf_cmd_timeout(union cfything *thing)
{
	char *endptr;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no timeout specified");
	default_timeout = strtol(cfytext, &endptr, 0);
	if (*endptr != '\0')
		errx(1, "bad timeout");
}

static void
conf_cmd_typemap_name(union cfything *thing)
{
	char *re;
	int type;
	
	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no re for typemap");
	re = malloc(cfyleng + 1);
	strcpy(re, cfytext);
	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no type for typemap");
	if (sscanf(cfytext, "%x", &type) != 1)
		errx(1, "bad type for typemap");
	if (fs_add_typemap_name(re, type) == -1)
		errx(1, "problem adding typemap");
}

static void
conf_cmd_typemap_perm(union cfything *thing)
{
	mode_t perm;
	int type;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no perm for typemap");
	if (sscanf(cfytext, "%o", &perm) != 1)
		errx(1, "bad perm for typemap");
	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no type for typemap");
	if (sscanf(cfytext, "%x", &type) != 1)
		errx(1, "bad type for typemap");
	if (fs_add_typemap_mode(perm, perm, type) == -1)
		errx(1, "problem adding typemap");
}

static void
conf_cmd_typemap_type(union cfything *thing)
{
	int type;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no type for typemap");
	if (sscanf(cfytext, "%x", &type) != 1)
		errx(1, "bad type for typemap");
	if (fs_add_typemap_mode(thing->func.mode, S_IFMT, type) == -1)
		errx(1, "problem adding typemap");
}

static void
conf_cmd_typemap_default(union cfything *thing)
{
	int type;

	if (cfylex(BORING, NULL) != CF_WORD)
		errx(1, "no type for typemap");
	if (sscanf(cfytext, "%x", &type) != 1)
		errx(1, "bad type for typemap");
	if (fs_add_typemap_default(type) == -1)
		errx(1, "problem adding typemap");
}
