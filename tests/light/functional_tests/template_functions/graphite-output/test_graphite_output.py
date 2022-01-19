#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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


def test_graphite_output(config, syslog_ng):
    template = "$(graphite-output --timestamp 'custom_timestamp' --key test.*)\n"

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(2)
    assert log == ["test.key1 value1 custom_timestamp\n", "test.key2 value2 custom_timestamp\n"]
