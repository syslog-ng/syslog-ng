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
SUBMODULES="lib/ivykis modules/afmongodb/mongo-c-driver/src/libbson modules/afmongodb/mongo-c-driver modules/afamqp/rabbitmq-c lib/jsonc"
GIT=`which git`

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
		git submodule update --init
		sed -e "s#git://#https://#" \
			< modules/afamqp/rabbitmq-c/.gitmodules \
			> modules/afamqp/rabbitmq-c/.gitmodules.new && \
			mv modules/afamqp/rabbitmq-c/.gitmodules.new modules/afamqp/rabbitmq-c/.gitmodules
		git submodule update --init --recursive
	fi

	for submod in $SUBMODULES; do
		echo "Running autogen in '$submod'..."
		cd "$submod"
		if [ -x autogen.sh ]; then
			# NOCONFIGURE needed by mongo-c-driver
			export NOCONFIGURE=1
			./autogen.sh
			unset NOCONFIGURE
		elif [ -f configure.in ] || [ -f configure.ac ]; then
			autoreconf -i
		else
			echo "Don't know how to bootstrap submodule '$submod'" >&2
			exit 1
		fi

		CONFIGURE_OPTS="--disable-shared --enable-static --with-pic"
		# kludge needed by make distcheck in mongo-c-driver
		CONFIGURE_OPTS="$CONFIGURE_OPTS --enable-man-pages"

		sed -e "s/@__CONFIGURE_OPTS__@/${CONFIGURE_OPTS}/g" ${origdir}/sub-configure.sh >configure.gnu
		cd "$origdir"
	done
}

if [ -z "$skip_submodules" ] || [ "$skip_modules" = 0 ]; then
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
