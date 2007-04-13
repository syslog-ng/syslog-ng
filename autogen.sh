#!/bin/sh
#
# $Id: autogen.sh,v 1.2 2004/08/20 21:22:34 bazsi Exp $
#
# This script is needed to setup build environment from checked out
# source tree. 
#

aclocal
autoheader
automake --foreign --add-missing
autoconf

