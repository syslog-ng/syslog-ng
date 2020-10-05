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
import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.operations import create_file
from src.helpers.loggen.loggen import Loggen

TEMPLATE = r'"${PROXIED_SRCIP} ${PROXIED_DSTIP} ${PROXIED_SRCPORT} ${PROXIED_DSTPORT} ${PROXIED_IP_VERSION} ${MESSAGE}\n"'

CLIENT_A_INPUT = "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n" \
                 "message A 0\n" \
                 "message A 1"
CLIENT_B_INPUT = "PROXY TCP4 5.5.5.5 6.6.6.6 7777 8888\r\n" \
                 "message B 0\n" \
                 "message B 1"

CLIENT_A_EXPECTED = (
    "1.1.1.1 2.2.2.2 3333 4444 4 message A 0\n",
    "1.1.1.1 2.2.2.2 3333 4444 4 message A 1\n",
)
CLIENT_B_EXPECTED = (
    "5.5.5.5 6.6.6.6 7777 8888 4 message B 0\n",
    "5.5.5.5 6.6.6.6 7777 8888 4 message B 1\n",
)


def test_pp_with_multiple_clients(config, port_allocator, syslog_ng):
    network_source = config.create_network_source(ip="localhost", port=port_allocator(), transport="proxied-tcp", flags="no-parse")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[network_source, file_destination])

    syslog_ng.start(config)

    client_A_loggen_input_path = str(tc_parameters.WORKING_DIR) + "/client_A_loggen_input.txt"
    client_B_loggen_input_path = str(tc_parameters.WORKING_DIR) + "/client_B_loggen_input.txt"

    create_file(client_A_loggen_input_path, CLIENT_A_INPUT)
    create_file(client_B_loggen_input_path, CLIENT_B_INPUT)

    # These 2 run simultaneously
    Loggen().start(
        network_source.options["ip"], network_source.options["port"], inet=True,
        stream=True, permanent=True, read_file=client_A_loggen_input_path, dont_parse=True, rate=1,
    )
    Loggen().start(
        network_source.options["ip"], network_source.options["port"], inet=True,
        stream=True, permanent=True, read_file=client_B_loggen_input_path, dont_parse=True, rate=1,
    )

    output_messages = file_destination.read_logs(counter=4)
    assert sorted(output_messages) == sorted(CLIENT_A_EXPECTED + CLIENT_B_EXPECTED)
