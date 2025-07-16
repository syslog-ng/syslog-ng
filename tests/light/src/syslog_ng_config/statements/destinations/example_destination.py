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
from pathlib import Path

from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver
from src.syslog_ng_ctl.legacy_stats_handler import LegacyStatsHandler
from src.syslog_ng_ctl.prometheus_stats_handler import PrometheusStatsHandler


class ExampleDestination(DestinationDriver):
    def __init__(
        self,
        stats_handler: LegacyStatsHandler,
        prometheus_stats_handler: PrometheusStatsHandler,
        filename: str,
        **options,
    ) -> None:
        self.driver_name = "example-destination"
        self.path = Path(filename)
        self.io = FileIO(self.path)
        super(ExampleDestination, self).__init__(
            stats_handler,
            prometheus_stats_handler,
            None,
            dict({"filename": self.path.resolve()}, **options),
        )

    def get_path(self):
        return self.path

    def read_log(self):
        return self.read_logs(1)[0]

    def read_logs(self, counter):
        return self.io.read_number_of_messages(counter)

    def read_until_logs(self, logs):
        return self.io.read_until_messages(logs)

    def close_file(self):
        self.io.close_readable_file()
