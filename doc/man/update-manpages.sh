#!/bin/sh
#############################################################################
# Copyright (c) 2017 Balabit
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

axosyslog_baseurl=https://axoflow.github.io/axosyslog-core-docs/syslog-ng-manpages
axosyslog_manpages="dqtool.1 \
	loggen.1 \
	pdbtool.1 \
	persist-tool.1 \
	syslog-ng-debun.1 \
	syslog-ng-ctl.1 \
	syslog-ng.8 \
	syslog-ng.conf.5 \
	slogencrypt.1 \
	slogkey.1 \
	slogverify.1 \
	secure-logging.7"

set -e
for man in ${axosyslog_manpages}; do
	url=${axosyslog_baseurl}/${man}
	echo "Downloading ${url}"
	if ! wget --quiet -O ${man}.in ${url}; then
		echo "Unable to download $man. Fix and re-run this script. Bailing out"
		exit 1
	fi
done
