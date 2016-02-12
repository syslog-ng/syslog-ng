#!/bin/sh
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
libtoolize --force --copy
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