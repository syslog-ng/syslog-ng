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
import pytest

from src.helpers.snmptrapd.conftest import *  # noqa:F403, F401


@pytest.mark.snmp
def test_snmp_dest_v2c_using_macros_in_snmp_obj(config, syslog_ng, snmptrapd, snmp_test_params):
    default_facility = "auth"
    default_severity = "0"
    program = "testprogram"
    message = "test message"
    input_message = "<38>Feb 11 21:27:22 testhost {}[9999]: {}\n".format(program, message)

    file_source = config.create_file_source(file_name="input.log")

    snmp_destination = config.create_snmp_destination(
        host=snmp_test_params.get_ip_address(),
        port=snmptrapd.get_port(),
        snmp_obj=(
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.2.55", "Octetstring", "$FACILITY"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.3.55", "Integer", "$SEVERITY"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.4.55", "Octetstring", "$PROGRAM"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.5.55", "Octetstring", "$MESSAGE"',
        ),
        trap_obj=snmp_test_params.get_cisco_trap_obj(),
        version="v2c",
    )
    config.create_logpath(statements=[file_source, snmp_destination])
    file_source.write_log(input_message)

    syslog_ng.start(config)

    expected_traps = [
        '.1.3.6.1.4.1.9.9.41.1.2.3.1.2.55 = STRING: "{}"'.format(default_facility),
        '.1.3.6.1.4.1.9.9.41.1.2.3.1.3.55 = INTEGER: {}'.format(default_severity),
        '.1.3.6.1.4.1.9.9.41.1.2.3.1.4.55 = STRING: "{}"'.format(program),
        '.1.3.6.1.4.1.9.9.41.1.2.3.1.5.55 = STRING: "{}"'.format(message),
        '.1.3.6.1.6.3.1.1.4.1.0 = OID: .1.3.6.1.4.1.9.9.41.2.0.1',
    ]

    assert expected_traps == snmptrapd.get_traps(len(expected_traps))
