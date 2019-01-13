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

from src.common.blocking import wait_until_false, wait_until_true
from src.syslog_ng_ctl.syslog_ng_ctl_executor import SyslogNgCtlExecutor


class SyslogNgCtlCli(object):
    def __init__(self, logger_factory, instance_paths):
        self.__syslog_ng_ctl_executor = SyslogNgCtlExecutor(logger_factory, instance_paths)

    def reload(self):
        return self.__syslog_ng_ctl_executor.run_command(command_short_name="reload", command=["reload"])

    def stop(self):
        return self.__syslog_ng_ctl_executor.run_command(command_short_name="stop", command=["stop"])

    def stats(self, reset):
        ctl_stats_command = self.__syslog_ng_ctl_executor.construct_ctl_stats_command(reset=reset)
        return self.__syslog_ng_ctl_executor.run_command(command_short_name="stats", command=ctl_stats_command)

    def is_control_socket_alive(self):
        return self.stats(reset=False)["exit_code"] == 0
