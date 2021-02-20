#! /bin/sh

rm -f Makefile
rm -f makefile

aclocal \
&& automake --gnu --add-missing \
&& autoconf

