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


def test_unix_stream_destination(config, syslog_ng):
    counter = 100
    message = "message text"

    generator_source = config.create_example_msg_generator_source(num=counter, freq=0.0001, template=config.stringify(message))
    unix_stream_destination = config.create_unix_stream_destination(file_name="output_unix_stream")
    config.create_logpath(statements=[generator_source, unix_stream_destination])

    unix_stream_destination.start_listener()
    syslog_ng.start(config)
    assert unix_stream_destination.read_until_logs([message] * counter)
    assert unix_stream_destination.get_stats()["written"] == counter
