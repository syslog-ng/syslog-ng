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

class SNMPTestParams(object):
    def __init__(self):
        pass

    def get_ip_address(self):
        return '"127.0.0.1"'

    def get_default_community(self):
        return 'public'

    def get_basic_snmp_obj(self):
        return '".1.3.6.1.4.1.18372.3.1.1.1.1.1.0", "Octetstring", "admin"'

    def get_basic_trap_obj(self):
        return '".1.3.6.1.6.3.1.1.4.1.0", "Objectid", ".1.3.6.1.4.1.18372.3.1.1.1.2.1"'

    def get_cisco_trap_obj(self):
        return '".1.3.6.1.6.3.1.1.4.1.0","Objectid",".1.3.6.1.4.1.9.9.41.2.0.1"'

    def get_cisco_snmp_obj(self):
        cisco_snmp_obj = (
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.2.55", "Octetstring", "SYS"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.3.55", "Integer", "6"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.4.55", "Octetstring", "CONFIG_I"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.5.55", "Octetstring", "Configured from console by vty1 (10.30.0.32)"',
            '"1.3.6.1.4.1.9.9.41.1.2.3.1.6.55", "Timeticks", "97881"',
        )
        return cisco_snmp_obj

    def get_expected_cisco_trap(self):
        return sorted([
            '.1.3.6.1.4.1.9.9.41.1.2.3.1.2.55 = STRING: "SYS"',
            '.1.3.6.1.4.1.9.9.41.1.2.3.1.3.55 = INTEGER: 6',
            '.1.3.6.1.4.1.9.9.41.1.2.3.1.4.55 = STRING: "CONFIG_I"',
            '.1.3.6.1.4.1.9.9.41.1.2.3.1.5.55 = STRING: "Configured from console by vty1 (10.30.0.32)"',
            '.1.3.6.1.4.1.9.9.41.1.2.3.1.6.55 = Timeticks: (97881) 0:16:18.81',
            '.1.3.6.1.6.3.1.1.4.1.0 = OID: .1.3.6.1.4.1.18372.3.1.1.1.2.1',
        ])

    def get_expected_basic_trap(self):
        return sorted([
            '.1.3.6.1.4.1.18372.3.1.1.1.1.1.0 = STRING: "admin"',
            '.1.3.6.1.6.3.1.1.4.1.0 = OID: .1.3.6.1.4.1.18372.3.1.1.1.2.1',
        ])

    def get_expected_empty_trap(self):
        return [
            '.1.3.6.1.6.3.1.1.4.1.0 = OID: .1.3.6.1.4.1.18372.3.1.1.1.2.1',
        ]


@pytest.fixture
def snmp_test_params():
    return SNMPTestParams()
