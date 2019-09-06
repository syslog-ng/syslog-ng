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


def test_fs_acceptance(config, syslog_ng, generate_msgs_for_file_src_and_dummy_dst):
    file_source = config.create_file_source(file_name="input.log")
    dummy_destination = config.create_dummy_destination()
    config.create_logpath(statements=[file_source, dummy_destination])
    config.update_global_options(keep_hostname="yes")

    input_messages, expected_messages = generate_msgs_for_file_src_and_dummy_dst

    file_source.write_log(input_messages[1])
    syslog_ng.start(config)
    assert dummy_destination.read_log() == expected_messages[1]
