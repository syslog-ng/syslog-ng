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

import pytest


def process_valgrind_log(valgrind_log_path):
    with open(valgrind_log_path, "r") as valgrind_log:
        valgrind_content = valgrind_log.read()
        assert "Invalid read" not in valgrind_content
        assert "Invalid write" not in valgrind_content


@pytest.mark.slow
def test_with_valgrind(tc):
    config = tc.new_config()

    file_source = config.create_file_source(file_name="input.log")
    source_group = config.create_source_group(file_source)

    file_destination = config.create_file_destination(file_name="output.log")
    destination_group = config.create_destination_group(file_destination)

    config.create_logpath(sources=source_group, destinations=destination_group)

    bsd_message = tc.create_dummy_bsd_message()
    bsd_log = tc.format_as_bsd(bsd_message)

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config, external_tool="valgrind")

    for __counter in range(1, 5):
        syslog_ng.reload(config)
        file_source.write_log(bsd_log, counter=100)
        syslog_ng.reload(config)

    syslog_ng.stop()

    valgrind_log_path = syslog_ng.get_instance_paths().get_valgrind_log_path()
    process_valgrind_log(valgrind_log_path)
