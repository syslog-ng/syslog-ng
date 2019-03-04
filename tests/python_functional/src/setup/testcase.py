#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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

from src.common.operations import copy_file
from src.setup.testcase_parameters import TestcaseParameters
from src.syslog_ng.syslog_ng_paths import SyslogNgPaths
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig
from src.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.syslog_ng.syslog_ng import SyslogNg
from src.message_builder.bsd_format import BSDFormat
from src.message_builder.log_message import LogMessage


class SetupTestCase(object):
    def __init__(self, testcase_context):
        self.__testcase_context = testcase_context
        self.testcase_parameters = TestcaseParameters(testcase_context)
        self.__prepare_testcase_working_dir()

        self.__teardown_actions = []
        testcase_context.addfinalizer(self.__teardown)

    def __prepare_testcase_working_dir(self):
        working_directory = self.testcase_parameters.get_working_dir()
        if not working_directory.exists():
            working_directory.mkdir(parents=True)
        testcase_file_path = self.testcase_parameters.get_testcase_file()
        copy_file(testcase_file_path, working_directory)

    def __teardown(self):
        for inner_function in self.__teardown_actions:
            try:
                inner_function()
            except OSError:
                pass

    def new_syslog_ng(self, instance_name="server"):
        instance_paths = SyslogNgPaths(self.testcase_parameters).set_syslog_ng_paths(
            instance_name
        )
        syslog_ng = SyslogNg(instance_paths, self.testcase_parameters)
        self.__teardown_actions.append(syslog_ng.stop)
        return syslog_ng

    def new_syslog_ng_ctl(self, syslog_ng):
        return SyslogNgCtl(syslog_ng.instance_paths)

    def new_config(self, version=None):
        if not version:
            version = self.__testcase_context.getfixturevalue("version")
        return SyslogNgConfig(self.testcase_parameters.get_working_dir(), version)

    @staticmethod
    def format_as_bsd(log_message):
        return BSDFormat().format_message(log_message)

    @staticmethod
    def format_as_bsd_logs(log_message, counter):
        return [BSDFormat().format_message(log_message)] * counter

    @staticmethod
    def new_log_message():
        return LogMessage()
