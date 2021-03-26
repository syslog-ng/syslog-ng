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
from src.common.file import copy_shared_file

TEMPLATE = r'"${PROXIED_SRCIP} ${PROXIED_DSTIP} ${PROXIED_SRCPORT} ${PROXIED_DSTPORT} ${PROXIED_IP_VERSION} ${MESSAGE}\n"'
INPUT_MESSAGES = "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n" \
                 "message 0"
EXPECTED_MESSAGES = "1.1.1.1 2.2.2.2 3333 4444 4 message 0\n"


def test_pp_tls(config, syslog_ng, port_allocator, loggen, testcase_parameters):
    server_key_path = copy_shared_file(testcase_parameters, "server.key")
    server_cert_path = copy_shared_file(testcase_parameters, "server.crt")

    network_source = config.create_network_source(
        ip="localhost",
        port=port_allocator(),
        transport='"proxied-tls"',
        flags="no-parse",
        tls={
            "key-file": server_key_path,
            "cert-file": server_cert_path,
            "peer-verify": '"optional-untrusted"',
        },
    )
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[network_source, file_destination])

    syslog_ng.start(config)

    network_source.write_log(INPUT_MESSAGES)

    assert file_destination.read_log() == EXPECTED_MESSAGES
