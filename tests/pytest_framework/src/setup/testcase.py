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
from src.logger.logger_factory import LoggerFactory
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig
from src.syslog_ng.syslog_ng import SyslogNg
from src.syslog_ng.syslog_ng_cli import SyslogNgCli
from src.message_builder.bsd_format import BSDFormat
from src.message_builder.log_message import LogMessage


class SetupTestCase(object):
    def __init__(self, testcase_context):
        self.__testcase_context = testcase_context
        self.__testcase_parameters = TestcaseParameters(testcase_context)
        self.__prepare_testcase_working_dir()

        self.__logger_factory = LoggerFactory(
            report_file=self.__testcase_parameters.get_report_file(), loglevel=self.__testcase_parameters.get_loglevel()
        )
        self.__logger = self.__logger_factory.create_logger("Setup", use_console_handler=True, use_file_handler=True)
        self.__instances = {}

        self.__teardown_actions = []
        testcase_context.addfinalizer(self.__teardown)
        self.__logger.info("Testcase setup finish:%s", self.__testcase_parameters.get_testcase_name())

    def __prepare_testcase_working_dir(self):
        working_directory = self.__testcase_parameters.get_working_dir()
        if not working_directory.exists():
            working_directory.mkdir(parents=True)
        testcase_file_path = self.__testcase_parameters.get_testcase_file()
        copy_file(testcase_file_path, working_directory)

    def __teardown(self):
        self.__logger = self.__logger_factory.create_logger("Teardown", use_console_handler=True, use_file_handler=True)
        self.__logger.info("Testcase teardown start:%s", self.__testcase_parameters.get_testcase_name())
        for inner_function in self.__teardown_actions:
            try:
                inner_function()
            except OSError:
                pass
        self.__log_assertion_error()
        self.__logger.info("Testcase teardown finish:%s", self.__testcase_parameters.get_testcase_name())

    def __log_assertion_error(self):
        terminalreporter = self.__testcase_context.config.pluginmanager.getplugin("terminalreporter")
        if terminalreporter.stats.get("failed"):
            for failed_report in terminalreporter.stats.get("failed"):
                if failed_report.location[2] == self.__testcase_context.node.name:
                    self.__logger = self.__logger_factory.create_logger(
                        "Teardown", use_console_handler=False, use_file_handler=True
                    )
                    self.__logger.error(str(failed_report.longrepr))

    def __is_instance_registered(self, instance_name):
        return instance_name in self.__instances.keys()

    def __register_instance(self, instance_name):
        instance_paths = SyslogNgPaths(self.__testcase_context, self.__testcase_parameters).set_syslog_ng_paths(
            instance_name
        )
        syslog_ng_cli = SyslogNgCli(self.__logger_factory, instance_paths)
        syslog_ng = SyslogNg(syslog_ng_cli)
        self.__teardown_actions.append(syslog_ng.stop)
        self.__instances.update({instance_name: {}})
        self.__instances[instance_name]["syslog-ng"] = syslog_ng
        self.__instances[instance_name]["config"] = SyslogNgConfig(
            self.__logger_factory, instance_paths, syslog_ng.get_version()
        )

    def new_config(self, instance_name="server"):
        if not self.__is_instance_registered(instance_name):
            self.__register_instance(instance_name)
        return self.__instances[instance_name]["config"]

    def new_syslog_ng(self, instance_name="server"):
        if not self.__is_instance_registered(instance_name):
            self.__register_instance(instance_name)
        return self.__instances[instance_name]["syslog-ng"]

    @staticmethod
    def format_as_bsd(log_message):
        return BSDFormat().format_message(log_message)

    @staticmethod
    def format_as_bsd_logs(log_message, counter):
        return [BSDFormat().format_message(log_message)] * counter

    @staticmethod
    def new_log_message():
        return LogMessage()
