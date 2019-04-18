#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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
import pytest


test_parameters_syslog = [
    ("<0>1 2012-03-05T15:10:34+02:00 localhost kernel 1234 - - transport_info='UDP: [10.30.35.100]:60513->[10.30.0.35]:162', DISMAN-EVENT-MIB::sysUpTimeInstance='(277195) 0:46:11.95', SNMPv2-MIB::snmpTrapOID.0='SNMPv2-SMI::enterprises.9.9.41.2.0.1.0', SNMPv2-SMI::enterprises.9.9.41.1.2.3.1.2.0='user', SNMPv2-SMI::enterprises.9.9.41.1.2.3.1.3.0='6', SNMPv2-SMI::enterprises.9.9.41.1.2.3.1.4.0='', SNMPv2-SMI::enterprises.9.9.41.1.2.3.1.5.0='[ 2771.344837] PF: filter/input DROP IN=eth0 OUT= MAC=08:00:27:d5:33:1e:34:e6:d7:1f:04:58:08:00 SRC=10.30.0.35 DST=10.30.35.100 LEN=60 TOS=0x00 PREC=0x00 TTL=1 ID=62309 DF PROTO=TCP SPT=52988 DPT=5355 WINDOW=29200 RES=0x00 SYN URGP=0 ', SNMPv2-SMI::enterprises.9.9.41.1.2.3.1.6.0='(277195) 0:46:11.95', SNMP-COMMUNITY-MIB::snmpTrapAddress.0='127.0.0.1'", "${.app.name}", "iptables"),
]
@pytest.mark.parametrize("input_message, template, expected_value", test_parameters_syslog,
                         ids=list(map(str, range(len(test_parameters_syslog)))))
def test_application_syslog(config, syslog_ng, input_message, template, expected_value):
    config.add_include("scl.conf")

    generator_source = config.create_example_msg_generator(num=1, template="\"" + input_message + "\"")
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog")
    file_destination = config.create_file_destination(file_name="output.log", template="\"" + template + "\\n" + "\"")
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log() == expected_value + "\n"
