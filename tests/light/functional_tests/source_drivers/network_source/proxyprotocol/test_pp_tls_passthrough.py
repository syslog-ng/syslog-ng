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
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.blocking import wait_until_true
from src.common.file import copy_shared_file
from src.common.file import File
from src.common.random_id import get_unique_id

TEMPLATE = r'"${PROXIED_SRCIP} ${PROXIED_DSTIP} ${PROXIED_SRCPORT} ${PROXIED_DSTPORT} ${PROXIED_IP_VERSION} ${MESSAGE}\n"'
INPUT_MESSAGES = "message 0\n\n"
EXPECTED_MESSAGES = "1.1.1.1 2.2.2.2 3333 4444 4 message 0\n"
NUMBER_OF_MESSAGES = 1


def test_pp_tls_passthrough(config, syslog_ng, port_allocator, loggen, testcase_parameters):
    server_key_path = copy_shared_file(testcase_parameters, "server.key")
    server_cert_path = copy_shared_file(testcase_parameters, "server.crt")

    network_source = config.create_network_source(
        ip="localhost",
        port=port_allocator(),
        transport='"proxied-tls-passthrough"',
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

    loggen_input_file_path = Path(tc_parameters.WORKING_DIR, "loggen_input_{}.txt".format(get_unique_id()))
    loggen_input_file = File(loggen_input_file_path)
    loggen_input_file.open(mode="w")
    loggen_input_file.write(INPUT_MESSAGES)
    loggen_input_file.close()

    loggen.start(
        network_source.options["ip"], network_source.options["port"], number=NUMBER_OF_MESSAGES,
        use_ssl=True, proxied_tls_passthrough=True, read_file=str(loggen_input_file_path),
        dont_parse=True, proxy_src_ip="1.1.1.1", proxy_dst_ip="2.2.2.2", proxy_src_port="3333",
        proxy_dst_port="4444",
    )

    wait_until_true(lambda: loggen.get_sent_message_count() == NUMBER_OF_MESSAGES)
    assert file_destination.read_log() == EXPECTED_MESSAGES
