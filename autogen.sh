#!/bin/sh
#
# This script is needed to setup build environment from checked out
# source tree. 
#
SUBMODULES="lib/ivykis modules/afmongodb/libmongo-client modules/afamqp/rabbitmq-c lib/jsonc"
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
			./autogen.sh
		elif [ -f configure.in ] || [ -f configure.ac ]; then
			autoreconf -i
		else
			echo "Don't know how to bootstrap submodule '$submod'" >&2
			exit 1
		fi

                CONFIGURE_OPTS="--disable-shared --enable-static --with-pic"

                cat >configure.gnu <<EOF
#!/bin/sh

CONFIGURE_OPTS="${CONFIGURE_OPTS}"

echo \$0

configure="\`dirname \$0\`/\`basename \$0 .gnu\`"
echo "Running: " \$configure \$@ \$CONFIGURE_OPTS
\$SHELL \$configure "\$@" \$CONFIGURE_OPTS
EOF
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
