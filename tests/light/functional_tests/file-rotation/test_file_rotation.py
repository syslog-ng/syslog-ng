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
import pytest


@pytest.mark.parametrize(
    "file_rotation", [
        "size(1), interval(daily), dateformat(-%Y-%m-%d)",
    ], ids=["file-rotation"],
)
def test_file_rotation(config, syslog_ng, port_allocator, file_rotation):
    counter = 100
    message = "message text"

    generator_source = config.create_example_msg_generator_source(num=counter, freq=0.0001, template=config.stringify(message))
    if file_rotation is not None:
        file_destination = config.create_network_destination(ip="localhost", port=port_allocator(), file_rotation=file_rotation)
    else:
        file_destination = config.create_network_destination(ip="localhost", port=port_allocator())
    config.create_logpath(statements=[generator_source, file_destination])

    file_destination.start_listener()
    syslog_ng.start(config)
    assert file_destination.read_until_logs([message] * counter)
