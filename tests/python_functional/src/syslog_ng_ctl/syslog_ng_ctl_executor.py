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
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.executors.command_executor import CommandExecutor


class SyslogNgCtlExecutor(object):
    def __init__(self, instance_paths):
        self.__instance_paths = instance_paths
        self.__syslog_ng_control_tool_path = instance_paths.get_syslog_ng_ctl_bin()
        self.__syslog_ng_control_socket_path = instance_paths.get_control_socket_path()
        self.__command_executor = CommandExecutor()

    def run_command(self, command_short_name, command):
        return self.__command_executor.run(
            command=self.__construct_ctl_command(command),
            stdout_path=self.__construct_std_file_path(command_short_name, "stdout"),
            stderr_path=self.__construct_std_file_path(command_short_name, "stderr"),
        )

    def __construct_std_file_path(self, command_short_name, std_type):
        instance_name = self.__instance_paths.get_instance_name()
        return Path(tc_parameters.WORKING_DIR, "syslog_ng_ctl_{}_{}_{}".format(instance_name, command_short_name, std_type))

    def __construct_ctl_command(self, command):
        ctl_command = [self.__syslog_ng_control_tool_path]
        ctl_command += command
        ctl_command.append("--control={}".format(self.__syslog_ng_control_socket_path))
        return ctl_command

    @staticmethod
    def construct_ctl_stats_command(reset):
        stats_command = ["stats"]
        if reset:
            stats_command.append("--reset")
        return stats_command
