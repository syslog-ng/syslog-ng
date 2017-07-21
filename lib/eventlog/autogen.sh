#!/bin/sh
#
# $Id: autogen.sh,v 1.2 2004/08/20 19:46:28 bazsi Exp $
#
# Run this script to generate Makefile skeletons and configure
# scripts.
#

libtoolize -f -c
aclocal -I m4 --install
autoheader
automake --add-missing --foreign --copy --force-missing
autoconf
