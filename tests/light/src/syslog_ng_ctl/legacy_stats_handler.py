#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Axoflow
# Copyright (c) 2025 Attila Szakacs <attila.szakacs@axoflow.com>
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
import re
import typing

from src.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl


class LegacyStatsHandler(object):
    def __init__(self, syslog_ng_ctl: SyslogNgCtl) -> None:
        self.__syslog_ng_ctl = syslog_ng_ctl

    def get_query(self, group_type: str, driver_name: str) -> typing.Dict[str, int]:
        component = LegacyStatsHandler.__build_component(group_type, driver_name)
        query_pattern = LegacyStatsHandler.__build_query_pattern(component)
        query_result = self.__syslog_ng_ctl.query(pattern="*{}*".format(query_pattern))['stdout'].splitlines()

        return self.__parse_result(result=query_result, result_type="query")

    def get_stats(self, group_type: str, driver_name: str, driver_instance: str = "") -> typing.Dict[str, int]:
        component = LegacyStatsHandler.__build_component(group_type, driver_name)
        stats_pattern = LegacyStatsHandler.__build_stats_pattern(component)
        stats_result = self.__syslog_ng_ctl.stats()['stdout'].splitlines()

        filtered_stats_result = LegacyStatsHandler.__filter_stats_result(stats_result, stats_pattern, driver_instance)
        return LegacyStatsHandler.__parse_result(result=filtered_stats_result, result_type="stats")

    @staticmethod
    def __build_component(group_type: str, driver_name: str) -> str:
        if group_type == "destination":
            statement_short_name = "dst"
            return "{}.{}".format(statement_short_name, driver_name)
        elif group_type == "source":
            statement_short_name = "src"
            return "{}.{}".format(statement_short_name, driver_name)
        elif group_type == "parser":
            return "{}".format(group_type)
        elif group_type == "filter":
            return "{}".format(group_type)
        else:
            raise Exception("Unknown group_type: {}".format(group_type))

    @staticmethod
    def __build_query_pattern(component: str) -> str:
        return LegacyStatsHandler.__build_pattern(component=component, delimiter=".")

    @staticmethod
    def __build_stats_pattern(component: str) -> str:
        return LegacyStatsHandler.__build_pattern(component=component, delimiter=";")

    @staticmethod
    def __build_pattern(component: str, delimiter: str) -> str:
        return f"{component}{delimiter}"

    @staticmethod
    def __parse_query_result(line: str) -> typing.Dict[str, int]:
        split = re.split(r"\.|=", line)
        if len(split) < 2:
            return dict()

        key, value = split[-2:]
        return {key: int(value)}

    @staticmethod
    def __parse_stats_result(line: str) -> typing.Dict[str, int]:
        split = line.split(";")
        if len(split) < 2:
            return dict()

        key, value = split[-2:]
        return {key: int(value)}

    @staticmethod
    def __parse_result(result: typing.List[str], result_type: str) -> typing.Dict[str, int]:
        parsed_output = {}
        for line in result:
            if result_type == "query":
                parsed_output.update(LegacyStatsHandler.__parse_query_result(line))
            elif result_type == "stats":
                parsed_output.update(LegacyStatsHandler.__parse_stats_result(line))
            else:
                raise Exception("Unknown result_type: {}".format(result_type))
        return parsed_output

    @staticmethod
    def __filter_stats_result(
        stats_result: typing.List[str],
        stats_pattern: str,
        driver_instance: str,
    ) -> typing.List[str]:
        filtered_list = []
        for stats_line in stats_result:
            if stats_pattern in stats_line and driver_instance in stats_line:
                filtered_list.append(stats_line)
        return filtered_list
