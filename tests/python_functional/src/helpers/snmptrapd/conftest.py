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
from src.driver_io.file.file_io import FileIO
from src.executors.process_executor import ProcessExecutor
from src.syslog_ng_config.statements.destinations.destination_reader import DestinationReader


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

    def get_traps(self):
        file_reader = DestinationReader(FileIO)
        logs = file_reader.read_all_logs(self.snmptrapd_log)
        trap_list = []
        for log_line in logs:
            res = re.match('({})(.*)'.format(self.TRAP_LOG_PREFIX), log_line)
            if (res):
                for trap in res.group(2).rstrip().split("\t"):
                    trap_list.append(trap)
        return trap_list


@pytest.fixture(scope="module")
def snmptrapd(some_port):
    server = SNMPtrapd(some_port)
    server.start()
    yield server
    server.stop()
