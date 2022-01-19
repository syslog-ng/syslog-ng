#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 One Identity
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
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
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

TEMPLATE = r'"${PROXIED_SRCIP} ${PROXIED_DSTIP} ${PROXIED_SRCPORT} ${PROXIED_DSTPORT} ${PROXIED_IP_VERSION} ${MESSAGE}\n"'
INPUT_MESSAGES = "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n" \
                 "message 0"
EXPECTED_MESSAGE0 = "1.1.1.1 2.2.2.2 3333 4444 4 message 0\n"


def test_pp_acceptance(config, syslog_ng, loggen, port_allocator):
    network_source = config.create_network_source(ip="localhost", port=port_allocator(), transport='"proxied-tcp"', flags="no-parse")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[network_source, file_destination])

    syslog_ng.start(config)

    network_source.write_log(INPUT_MESSAGES)

    assert file_destination.read_log() == EXPECTED_MESSAGE0
