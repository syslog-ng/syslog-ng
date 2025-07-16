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
import typing

from src.executors.command_executor import CommandExecutor
from src.syslog_ng_ctl.syslog_ng_ctl_executor import SyslogNgCtlExecutor


class SyslogNgCtlDockerExecutor(SyslogNgCtlExecutor):
    def __init__(self, container_name: str) -> None:
        self.__container_name = container_name
        self.__command_executor = CommandExecutor()

    def run_command(
        self,
        instance_name: str,
        command_short_name: str,
        command: typing.List[str],
    ) -> typing.Dict[str, typing.Any]:
        ctl_command = ["docker", "exec", self.__container_name, "syslog-ng-ctl"]
        ctl_command += ["--control=/tmp/syslog-ng.ctl"]
        ctl_command += command

        return self.__command_executor.run(
            command=ctl_command,
            stdout_path=self.construct_std_file_path(instance_name, command_short_name, "stdout"),
            stderr_path=self.construct_std_file_path(instance_name, command_short_name, "stderr"),
        )
