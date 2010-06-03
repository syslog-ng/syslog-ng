#!/bin/sh
#############################################################################
# Copyright (c) 2010 BalaBit IT Ltd, Budapest, Hungary
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#
# Note that this permission is granted for only version 2 of the GPL.
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
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#############################################################################


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
	HP-UX)
		cat <<EOF
pipe("/dev/log" pad_size(2048));
EOF
		;;
	AIX|OSF1)
		cat <<EOF
unix-dgram("/dev/log");
EOF
		;;
esac
