#!/bin/sh
#
# $Id: autogen.sh,v 1.2 2004/08/20 21:22:34 bazsi Exp $
#
# This script is needed to setup build environment from checked out
# source tree. 
#
SUBMODULES="lib/ivykis modules/afmongodb/libmongo-client"
GIT=`which git`

autogen_submodules()
{
	origdir=`pwd`

	submod_initialized=0
	for submod in $SUBMODULES; do
		if [ -f $submod/configure.gnu ]; then
			submod_initialized=1
		fi
	done

	if [ -n "$GIT" ] && [ -f .gitmodules ] && [ -d .git ] && [ $submod_initialized = 0 ]; then
		# only clone submodules if none of them present
		git submodule update --init --recursive
	fi

	for submod in $SUBMODULES; do
		echo "Running autogen in '$submod'..."
		cd "$submod"
		if [ -x autogen.sh ]; then
			./autogen.sh
		elif [ -f configure.in ] || [ -f configure.ac ]; then
			autoreconf
		else
			echo "Don't know how to bootstrap submodule '$submod'" >&2
			exit 1
		fi
		cd "$origdir"
	done
}

if [ -z "$skip_submodules" ] || [ "$skip_modules" = 0 ]; then
	autogen_submodules
fi

# bootstrap syslog-ng itself
libtoolize --force
aclocal -I m4 --install
sed -i -e 's/PKG_PROG_PKG_CONFIG(\[0\.16\])/PKG_PROG_PKG_CONFIG([0.14])/g' aclocal.m4

autoheader
automake --foreign --add-missing
autoconf
