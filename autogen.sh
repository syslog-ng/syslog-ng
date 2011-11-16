#!/bin/sh
#
# $Id: autogen.sh,v 1.2 2004/08/20 21:22:34 bazsi Exp $
#
# This script is needed to setup build environment from checked out
# source tree. 
#
DUMMY_SUBMODULES="lib/ivykis modules/afmongodb/libmongo-client"

set -e
autogen_submodules()
{

	origdir=`pwd`

	for submod in $SUBMODULES; do
		if [ -f .gitmodules ]; then
			git submodule update --init --recursive -- $submod || git submodule update --init -- $submod
		fi

		echo "Running autogen in '$submod'..."
		cd "$submod"
		git checkout master
		git pull
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
if [ "$USER" = "testbot" ];then
	skip_submodules=1
fi

for submod in $DUMMY_SUBMODULES; do
	mkdir -p $submod
done

(
 pemodpath="$ZWA_ROOT/git/syslog-ng/syslog-ng-pe-modules--mainline--4.2/modules"
 for pemod in license logstore diskq confighash snmp afsqlsource; do
    if [ -d $pemodpath/$pemod ]; then
        if [ -h modules/$pemod ] || [ -d modules/$pemod ]; then rm -rf modules/$pemod; fi
        ln -s $pemodpath/$pemod modules/$pemod
    fi
 done
 petests_orig="$ZWA_ROOT/git/syslog-ng/syslog-ng-pe-modules--mainline--4.2/tests"
 petests="pe-tests"
 if [ -d $petests_orig ]; then
     if [ -h $petests ] || [ -d $petests ]; then rm -rf $petests; fi
     ln -s $petests_orig $petests
 fi
)
# bootstrap syslog-ng itself
libtoolize --force
aclocal -I m4 --install
sed -i -e 's/PKG_PROG_PKG_CONFIG(\[0\.16\])/PKG_PROG_PKG_CONFIG([0.14])/g' aclocal.m4

autoheader
automake --foreign --add-missing --copy
autoconf
