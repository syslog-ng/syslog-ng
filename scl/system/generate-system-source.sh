#!/bin/sh
#############################################################################
# Copyright (c) 2010 BalaBit IT Ltd, Budapest, Hungary
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#############################################################################

# NOTE: the reason to set PATH and LD_LIBRARY_PATH/LIBPATH explicitly
# is that some OSes (notably AIX) can have multiple uname binaries,
# and the one on the PATH used by syslog-ng may depend on libraries
# that syslog-ng itself is distributing. However the one distributed
# by the syslog-ng binary is binary incompatible with the one required
# by the alternative uname, causing uname execution to fail.
#
# The alternative, better solution would be to patch syslog-ng to
# remove its own changes from LD_LIBRARY_PATH before executing user
# supplied scripts, but that was more involved than this simple change
# here.

PATH=/bin:/usr/bin:$PATH
LIBPATH=
LD_LIBRARY_PATH=
export PATH LIBPATH LD_LIBRARY_PATH

os=${UNAME_S:-`uname -s`}
osversion=${UNAME_R:-`uname -r`}

case $os in
	Linux)
		cat <<EOF
unix-dgram("/dev/log");
file("/proc/kmsg" program-override("kernel") flags(kernel));
EOF
		;;
	SunOS)
		if [ "$osversion" = "5.8" ]; then
			cat <<EOF
@module afstreams
sun-streams("/dev/log");
EOF
		elif [ "$osversion" = "5.9" ]; then
			cat <<EOF
@module afstreams
sun-streams("/dev/log" door("/etc/.syslog_door"));
EOF
		else
			cat <<EOF
@module afstreams
sun-streams("/dev/log" door("/var/run/syslog_door"));
EOF
		fi
		;;
	FreeBSD)
		cat <<EOF
unix-dgram("/var/run/log");
unix-dgram("/var/run/logpriv" perm(0600));
file("/dev/klog" follow-freq(0) program-override("kernel") flags(no-parse));
EOF
		;;
	GNU/kFreeBSD)
		cat <<EOF
unix-dgram("/var/run/log");
file("/dev/klog" follow-freq(0) program-override("kernel"));
EOF
		;;
	HP-UX)
		cat <<EOF
pipe("/dev/log" pad_size(2048));
EOF
		;;
	AIX|OSF1|CYGWIN*)
		cat <<EOF
unix-dgram("/dev/log");
EOF
		;;
	*)
		# need to notify the user that something went terribly wrong...
	        (echo "system(): Error detecting platform, unable to define the system() source.";
                 echo "";
                 echo "The script generating the syslog-ng configuration for the system() source failed.";
                 echo "It is caused either by an unsupported OS or the fact that the script could not";
                 echo "execute uname(1).";
                 echo "";
                 echo "    OS=${os}";
                 echo "    OS_VERSION=${osversion}";
                 echo "    SCRIPT=$0") >&2

		exit 1
		;;
esac
