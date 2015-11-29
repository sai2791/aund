# Copyright (c) 2010 Simon Tatham
# Copyright (c) 2010 Ben Harris
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

bin_PROGRAMS = aund
man_MANS = aund.conf.5 aund.passwd.5 aund.8
aund_SOURCES = extern.h aund.c \
	fileserver.h fs_errors.h fs_proto.h \
	fileserver.c fs_cli.c fs_examine.c \
	fs_fileio.c fs_misc.c fs_handle.c fs_util.c fs_error.c \
	fs_nametrans.c fs_filetype.c \
	aun.h aun.c beebem.c pw.c user_null.c \
	version.h
aund_LDADD = libconf_lex.a $(LIBOBJS)
AM_CFLAGS = $(GCCWARNINGS)

# conf_lex.l goes into a trivial library file and is then linked
# into the main program. This is because it needs slightly different
# gcc warning options (the fixed code imported by flex doesn't obey
# all the cleanliness properties we enforce for aund proper), and
# the automake docs say that per-file options aren't supported -
# only per-project, so any file that needs special options has to be
# turned into a small sub-project like this.

libconf_lex_a_SOURCES = conf_lex.l
libconf_lex_a_CFLAGS = $(GCCFEWERWARNINGS)
noinst_LIBRARIES = libconf_lex.a

EXTRA_DIST = contrib aund.conf.example $(man_MANS)
