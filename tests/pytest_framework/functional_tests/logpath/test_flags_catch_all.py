#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2019 Balabit
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


def write_dummy_message(tc, file_source):
    bsd_message = tc.new_log_message()
    bsd_log = tc.format_as_bsd(bsd_message)
    file_source.write_log(bsd_log)
    return bsd_message


def test_flags_catch_all(tc):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { destination(d_file1); };
    # };
    # log { destination(d_file2); flags(catch-all);};
    # input logs:
    # Oct 11 22:14:15 host-A testprogram: message from host-A

    config = tc.new_config()

    file_source = config.create_file_source(file_name="input.log")
    source_group = config.create_statement_group(file_source)

    file_destination1 = config.create_file_destination(file_name="output1.log")
    destination_group = config.create_statement_group(file_destination1)

    file_destination2 = config.create_file_destination(file_name="output2.log")
    catch_all_destination = config.create_statement_group(file_destination2)

    inner_logpath = config.create_inner_logpath(statements=[destination_group])

    config.create_logpath(statements=[source_group, inner_logpath])
    config.create_logpath(statements=[catch_all_destination], flags="catch-all")

    bsd_message = write_dummy_message(tc, file_source)

    expected_bsd_message = bsd_message.remove_priority()

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)

    dest1_logs = file_destination1.read_log()
    # message should arrived into destination1
    assert tc.format_as_bsd(expected_bsd_message) in dest1_logs

    dest2_logs = file_destination2.read_log()
    # message should arrived into destination2
    # there is a flags(catch-all)
    assert tc.format_as_bsd(expected_bsd_message) in dest2_logs
