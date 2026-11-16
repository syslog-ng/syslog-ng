#!/usr/bin/env python
#############################################################################
# Copyright (c) 2026 C3B2W23 <217007207+C3B2W23@users.noreply.github.com>
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
import json
import re

import pytest


@pytest.mark.parametrize(
    "frac_digits, timestamp_pattern", [
        (None, r'"timestamp":\d+[,}]'),
        (3, r'"timestamp":\d+\.\d{3}[,}]'),
    ], ids=["default", "frac_digits_3"],
)
def test_format_gelf(config, syslog_ng, frac_digits, timestamp_pattern):
    config.add_include("scl.conf")
    if frac_digits is not None:
        config.update_global_options(frac_digits=frac_digits)

    generator_source = config.create_example_msg_generator_source(num=1)
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$(format-gelf)\n"))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)[0].rstrip("\n")

    assert log.endswith("\0")
    gelf = json.loads(log[:-1])
    assert gelf["version"] == "1.1"
    assert gelf["short_message"] == "-- Generated message. --"
    assert isinstance(gelf["level"], int)
    assert isinstance(gelf["timestamp"], (int, float))
    assert re.search(timestamp_pattern, log)
