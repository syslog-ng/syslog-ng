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
from abc import ABC
from abc import abstractmethod
from enum import Enum
from pathlib import Path


class QueryTypes(Enum):
    QUERY_GET = 0
    QUERY_SUM = 1
    QUERY_LIST = 2
    QUERY_GET_RESET = 3


class SyslogNgCtlExecutor(ABC):
    @abstractmethod
    def run_command(
        self,
        instance_name: str,
        command_short_name: str,
        command: typing.List[str],
    ) -> typing.Dict[str, typing.Any]:
        pass

    @staticmethod
    def construct_ctl_stats_command(reset):
        stats_command = ["stats"]
        if reset:
            stats_command.append("--reset")
        return stats_command

    @staticmethod
    def construct_ctl_stats_prometheus_command():
        return ["stats", "prometheus"]

    @staticmethod
    def construct_ctl_credentials_command(credential, secret):
        return ["credentials", "add", credential, secret]

    @staticmethod
    def construct_ctl_query_command(pattern, query_type):
        query_command = ["query"]

        if query_type == QueryTypes.QUERY_GET:
            query_command += ["get"]
        elif query_type == QueryTypes.QUERY_SUM:
            query_command += ["get --sum"]
        elif query_type == QueryTypes.QUERY_LIST:
            query_command += ["list"]
        elif query_type == QueryTypes.QUERY_GET_RESET:
            query_command += ["get --reset"]

        query_command += [pattern]

        return query_command

    @staticmethod
    def construct_std_file_path(instance_name: str, command_short_name: str, std_type: str) -> Path:
        return Path("syslog_ng_ctl_{}_{}_{}".format(instance_name, command_short_name, std_type))
