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
import os
import re

import pytest
from pathlib2 import Path
from psutil import TimeoutExpired

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.blocking import wait_until_true
from src.common.file import File
from src.executors.process_executor import ProcessExecutor


class SNMPtrapd(object):
    TRAP_LOG_PREFIX = 'LIGHT_TEST_SNMP_TRAP_RECEIVED:'

    def __init__(self, port):
        self.snmptrapd_proc = None
        self.port = port

        self.snmptrapd_log = Path(tc_parameters.WORKING_DIR, "snmptrapd_log")
        self.snmptrapd_stdout_path = Path(tc_parameters.WORKING_DIR, "snmptrapd_stdout")
        self.snmptrapd_stderr_path = Path(tc_parameters.WORKING_DIR, "snmptrapd_stderr")

    def wait_for_snmptrapd_log_creation(self):
        return self.snmptrapd_log.exists()

    def wait_for_snmptrapd_startup(self):
        return "NET-SNMP version" in self.snmptrapd_log.read_text()

    def start(self):
        if self.snmptrapd_proc is not None:
            return

        self.snmptrapd_proc = ProcessExecutor().start(
            [
                "snmptrapd", "-f",
                "--disableAuthorization=yes",
                "-C",
                "-m ALL",
                "-A",
                "-Ddump",
                "-On",
                "--doNotLogTraps=no",
                "--authCommunity=log public",
                self.port,
                "-d",
                "-Lf", os.path.relpath(str(self.snmptrapd_log)),
                "-F", "{}%v\n".format(self.TRAP_LOG_PREFIX),
            ],
            self.snmptrapd_stdout_path,
            self.snmptrapd_stderr_path,
        )
        wait_until_true(self.wait_for_snmptrapd_log_creation)
        wait_until_true(self.wait_for_snmptrapd_startup)
        return self.snmptrapd_proc.is_running()

    def stop(self):
        if self.snmptrapd_proc is None:
            return

        self.snmptrapd_proc.terminate()
        try:
            self.snmptrapd_proc.wait(4)
        except TimeoutExpired:
            self.snmptrapd_proc.kill()

        self.snmptrapd_proc = None

    def get_port(self):
        return self.port

    def get_traps(self, counter):
        trap_list = []

        f = File(self.snmptrapd_log)
        f.open("r")

        while True:
            trap_line = f.wait_for_lines([self.TRAP_LOG_PREFIX])[0]
            res = re.match('({})(.*)'.format(self.TRAP_LOG_PREFIX), trap_line)
            if (res):
                trap_list.extend(res.group(2).rstrip().split("\t"))
            if len(trap_list) == counter:
                break

        f.close()

        return sorted(trap_list)

    def get_log(self):
        f = File(self.snmptrapd_log)
        f.open("r")

        log = f.read()

        f.close()

        return log


@pytest.fixture
def snmptrapd(port_allocator):
    server = SNMPtrapd(port_allocator())
    server.start()
    yield server
    server.stop()


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
