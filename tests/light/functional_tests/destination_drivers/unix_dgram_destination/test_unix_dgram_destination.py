#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 One Identity
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
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters


def test_unix_dgram_destination(config, syslog_ng):
    counter = 100
    message = "message text"

    generator_source = config.create_example_msg_generator_source(num=counter, freq=0.0001, template=config.stringify(message))
    # NOTE: In this iteration testcase is responsible for generating proper output path (because of socket path length), this will be changed in the future
    relative_unix_dgram_path = Path(tc_parameters.WORKING_DIR, "output_unix_dgram").relative_to(Path.cwd())
    unix_dgram_destination = config.create_unix_dgram_destination(file_name=relative_unix_dgram_path)
    config.create_logpath(statements=[generator_source, unix_dgram_destination])

    unix_dgram_destination.start_listener()
    syslog_ng.start(config)
    assert unix_dgram_destination.read_until_logs([message] * counter)
    assert unix_dgram_destination.get_stats()["written"] == counter
