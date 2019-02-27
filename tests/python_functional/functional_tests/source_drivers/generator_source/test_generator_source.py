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


def test_generator_source(tc):
    config = tc.new_config()

    generator_source = config.create_example_msg_generator(num=1)
    file_destination = config.create_file_destination(file_name="output.log", template="'$MSG\n'")

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)
    log = file_destination.read_log()
    assert log == generator_source.DEFAULT_MESSAGE + "\n"
