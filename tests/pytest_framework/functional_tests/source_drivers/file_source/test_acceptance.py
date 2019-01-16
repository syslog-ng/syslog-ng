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


def test_acceptance(tc):
    config = tc.new_config()

    file_source = config.create_file_source(file_name="input.log")
    file_destination = config.create_file_destination(file_name="output.log")

    config.create_logpath(statements=[file_source, file_destination])

    log_message = tc.new_log_message()
    bsd_log = tc.format_as_bsd(log_message)
    file_source.write_log(bsd_log, counter=3)

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)

    output_logs = file_destination.read_logs(counter=3)
    expected_output_message = log_message.remove_priority()
    assert output_logs == tc.format_as_bsd_logs(expected_output_message, counter=3)
