#!/bin/sh
#
# $Id: autogen.sh,v 1.2 2004/08/20 21:22:34 bazsi Exp $
#
# This script is needed to setup build environment from checked out
# source tree. 
#

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

(
 pemodpath="$ZWA_ROOT/git/syslog-ng/syslog-ng-pe-modules--mainline--5.0/modules"
 for pemod in license logstore diskq confighash snmp afsqlsource rltp-proto; do
    if [ -d $pemodpath/$pemod ]; then
        if [ -h modules/$pemod ] || [ -d modules/$pemod ]; then rm -rf modules/$pemod; fi
        ln -s $pemodpath/$pemod modules/$pemod
    fi
 done
 petests_orig="$ZWA_ROOT/git/syslog-ng/syslog-ng-pe-modules--mainline--5.0/tests"
 petests="pe-tests"
 if [ -d $petests_orig ]; then
     if [ -h $petests ] || [ -d $petests ]; then rm -rf $petests; fi
     ln -s $petests_orig $petests
 fi
)
ACLOCALPATHS=
for pth in /opt/libtool/share/aclocal /usr/local/share/aclocal; do
	if [ -d $pth ];then
		ACLOCAPATHS="$ACLOCALPATHS -I $pth"
	fi
done
# bootstrap syslog-ng itself
libtoolize --force --copy
aclocal -I m4 $ACLOCALPATHS --install
sed -i -e 's/PKG_PROG_PKG_CONFIG(\[0\.16\])/PKG_PROG_PKG_CONFIG([0.14])/g' aclocal.m4

autoheader
automake --foreign --add-missing --copy
autoconf
find -name libtool -o -name ltmain.sh | xargs sed -i -e "s,'file format pe-i386.*\?','file format \(pei\*-i386\(\.\*architecture: i386\)\?|pe-arm-wince|pe-x86-64\)',"
sed -i -e "s, cmd //c, sh -c," ltmain.sh
