#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2019 Balabit
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


class Filter(object):
    group_type = "filter"

    def __init__(
        self,
        driver_name: str,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        positional_parameters,
        **options,
    ) -> None:
        self.options = options
        self.driver_name = driver_name
        self.stats_handler = stats_handler
        self.prometheus_stats_handler = prometheus_stats_handler
        self.positional_parameters = positional_parameters

    def get_stats(self):
        return self.stats_handler.get_stats(self.group_type, self.driver_name)

    def get_query(self):
        return self.stats_handler.get_query(self.group_type, self.driver_name)


class Match(Filter):
    def __init__(
        self,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        match_string: str,
        **options,
    ) -> None:
        super(Match, self).__init__("match", stats_handler, prometheus_stats_handler, [match_string], **options)


class RateLimit(Filter):
    def __init__(
        self,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        **options,
    ) -> None:
        super(RateLimit, self).__init__("rate-limit", stats_handler, prometheus_stats_handler, [], **options)
