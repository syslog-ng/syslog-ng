#!/bin/sh
#############################################################################
# Copyright (c) 2007-2016 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# This script is needed to setup build environment from checked out
# source tree.
#
SUBMODULES="lib/ivykis lib/jsonc"
GIT=`which git`

include_automake_from_dir_if_exists()
{
  local dir=$1
  if [ -f "$1/Makefile.am" ];
  then
    grep "include $dir/Makefile.am" Makefile.am
    if [ "$?" -eq "1" ];
    then
      last_include=$(grep ^include Makefile.am|grep Makefile.am|tail -n 1)
      sed -i s@"$last_include"@"$last_include\ninclude $dir/Makefile.am"@g Makefile.am
    fi
  fi
}


autogen_submodules()
{
	origdir=`pwd`

	submod_initialized=1
	for submod in $SUBMODULES; do
		if [ ! -f $submod/configure.gnu ]; then
			submod_initialized=0
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
			autoreconf -i
		else
			echo "Don't know how to bootstrap submodule '$submod'" >&2
			exit 1
		fi

		CONFIGURE_OPTS="--disable-shared --enable-static --with-pic"

		sed -e "s/@__CONFIGURE_OPTS__@/${CONFIGURE_OPTS}/g" ${origdir}/sub-configure.sh >configure.gnu
		cd "$origdir"
	done
}

if [ -z "$skip_submodules" ]; then
	autogen_submodules
fi

# bootstrap syslog-ng itself
case `uname -s` in
	"Darwin") LIBTOOLIZE="glibtoolize" ;;
	*) LIBTOOLIZE="libtoolize" ;;
esac

$LIBTOOLIZE --force --copy
aclocal -I m4 --install
sed -i -e 's/PKG_PROG_PKG_CONFIG(\[0\.16\])/PKG_PROG_PKG_CONFIG([0.14])/g' aclocal.m4

include_automake_from_dir_if_exists debian
include_automake_from_dir_if_exists tgz2build

autoheader
automake --foreign --add-missing --copy
autoconf

if grep AX_PREFIX_CONFIG_H configure > /dev/null; then
	cat <<EOF

You need autoconf-archive http://savannah.gnu.org/projects/autoconf-archive/
installed in order to generate the configure script, e.g:
apt-get install autoconf-archive

EOF
	exit 1
fi
