#!/bin/sh
[ -e config.cache ] && rm -f config.cache

libtoolize --automake
#gtkdocize || exit 1
aclocal
autoconf
autoheader
automake -a
./configure $@
exit

