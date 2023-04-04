#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Attila Szakacs
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
from dataclasses import dataclass

from src.syslog_ng_config.statements.logpath.logpath import LogPath


@dataclass
class ExpectedLogPathStats:
    logpath: LogPath
    ingress: int
    egress: int


def assert_logpath_stats(logpath, ingress, egress):
    stats = logpath.get_prometheus_stats()

    found = False
    for stat in stats:
        if stat.name == "syslogng_route_ingress_total" and stat.labels == {"id": logpath.name}:
            found = True
            actual_ingress = stat.value
    assert found, "Did not find ingress stats counter for logpath: %s" % (logpath.name)

    found = False
    for stat in stats:
        if stat.name == "syslogng_route_egress_total" and stat.labels == {"id": logpath.name}:
            found = True
            actual_egress = stat.value
    assert found, "Did not find egress stats counter for logpath: %s" % (logpath.name)

    assert actual_ingress == ingress, "Unexpected ingress stat. Logpath: %s Expected: %d Actual: %d" % (logpath.name, ingress, actual_ingress)
    assert actual_egress == egress, "Unexpected egress stat. Logpath: %s Expected: %d Actual: %d" % (logpath.name, egress, actual_egress)


def assert_multiple_logpath_stats(expected_logpath_stats):
    for _, expected_logpath_stat in expected_logpath_stats.items():
        assert_logpath_stats(expected_logpath_stat.logpath, expected_logpath_stat.ingress, expected_logpath_stat.egress)
