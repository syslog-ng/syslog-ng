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
from src.syslog_ng_ctl.legacy_stats_handler import LegacyStatsHandler
from src.syslog_ng_ctl.prometheus_stats_handler import PrometheusStatsHandler


class DestinationDriver(object):
    group_type = "destination"
    driver_instance = ""

    def __init__(
        self,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        positional_parameters=None,
        options=None,
    ):
        self.stats_handler = stats_handler
        self.prometheus_stats_handler = prometheus_stats_handler

        if positional_parameters is None:
            positional_parameters = []
        self.positional_parameters = positional_parameters
        if options is None:
            options = {}
        self.options = options

    def get_stats(self):
        return self.stats_handler.get_stats(self.group_type, self.driver_name, self.driver_instance)

    def get_query(self):
        return self.stats_handler.get_query(self.group_type, self.driver_name)
