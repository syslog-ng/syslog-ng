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
from src.syslog_ng.syslog_ng_paths import SyslogNgPaths
from src.syslog_ng_ctl.syslog_ng_ctl_executor import QueryTypes
from src.syslog_ng_ctl.syslog_ng_ctl_executor import SyslogNgCtlExecutor


class SyslogNgCtl(object):
    def __init__(self, instance_paths: SyslogNgPaths, syslog_ng_ctl_executor: SyslogNgCtlExecutor) -> None:
        self.__instance_name = instance_paths.get_instance_name()
        self.__syslog_ng_ctl_executor = syslog_ng_ctl_executor

    def reload(self):
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="reload",
            command=["reload"],
        )

    def stop(self):
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="stop",
            command=["stop"],
        )

    def stats(self, reset=False):
        ctl_stats_command = self.__syslog_ng_ctl_executor.construct_ctl_stats_command(reset=reset)
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="stats",
            command=ctl_stats_command,
        )

    def stats_prometheus(self):
        ctl_stats_prometheus_command = self.__syslog_ng_ctl_executor.construct_ctl_stats_prometheus_command()
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="stats_prometheus",
            command=ctl_stats_prometheus_command,
        )

    def query(self, pattern="*", query_type=QueryTypes.QUERY_GET):
        ctl_query_command = self.__syslog_ng_ctl_executor.construct_ctl_query_command(pattern, query_type)
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="query",
            command=ctl_query_command,
        )

    def credentials_add(self, credential, secret):
        ctl_credentials_command = self.__syslog_ng_ctl_executor.construct_ctl_credentials_command(credential, secret)
        return self.__syslog_ng_ctl_executor.run_command(
            self.__instance_name,
            command_short_name="credentials",
            command=ctl_credentials_command,
        )

    def is_control_socket_alive(self):
        return self.stats(reset=False)["exit_code"] == 0
