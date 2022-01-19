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
def test_snmp_dest_missing_snmp_obj(config, syslog_ng, snmptrapd, snmp_test_params):
    generator_source = config.create_example_msg_generator_source(num=1)
    snmp_destination = config.create_snmp_destination(
        host=snmp_test_params.get_ip_address(),
        port=snmptrapd.get_port(),
        trap_obj=snmp_test_params.get_basic_trap_obj(),
    )
    config.create_logpath(statements=[generator_source, snmp_destination])

    syslog_ng.start(config)

    expected_traps = snmp_test_params.get_expected_empty_trap()
    assert expected_traps == snmptrapd.get_traps(len(expected_traps))
