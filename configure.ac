dnl Copyright (c) 2010 Ben Harris
dnl Copyright (c) 2010 Simon Tatham
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl 3. The name of the author may not be used to endorse or promote products
dnl    derived from this software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
dnl IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
dnl OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
dnl INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
dnl NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
dnl THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

define(aund_version,esyscmd([perl -ne '/MAJOR\s*0x(.)/ and print "$1"; /MINOR\s*0x(..)/ and print ".$1";' version.h]))
AC_INIT([aund], aund_version, [bjh21@bjh21.me.uk])
AC_CONFIG_SRCDIR([aund.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_REQUIRE_AUX_FILE([INSTALL])
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_RANLIB
AC_PROG_INSTALL
AM_PROG_LEX
AC_CHECK_HEADERS([crypt.h])
AC_CHECK_MEMBERS([struct stat.st_mtimensec,
		  struct stat.st_mtim,
		  struct stat.st_birthtime])
AC_CONFIG_HEADERS([config.h])
AC_SEARCH_LIBS(crypt, crypt)
AC_CONFIG_FILES([Makefile])
if test "x$GCC" = "xyes"; then
  :
  AC_SUBST(GCCWARNINGS, ['-Wall -Wdeclaration-after-statement -Wold-style-definition -Wmissing-prototypes -Wredundant-decls -Wno-pointer-sign -Wno-uninitialized'])
  AC_SUBST(GCCFEWERWARNINGS, ['$(GCCWARNINGS) -Wno-old-style-definition -Wno-redundant-decls -Wno-unused'])
else
  :
  AC_SUBST(GCCWARNINGS, [])
  AC_SUBST(GCCFEWERWARNINGS, [])
fi
AC_OUTPUT
