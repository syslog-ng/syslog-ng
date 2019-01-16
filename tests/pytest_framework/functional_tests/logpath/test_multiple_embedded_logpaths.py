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


def write_message_with_fields(tc, file_source, hostname, program, message):
    bsd_message = tc.new_log_message().hostname(hostname).program(program).message(message)
    bsd_log = tc.format_as_bsd(bsd_message)
    file_source.write_log(bsd_log)
    return bsd_message


def test_multiple_embedded_logpaths(tc):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { filter{host("host-A")}; destination(d_file1); };
    #     log { filter{program("app-A")}; destination(d_file2); };
    #     log { destination(d_file3); };
    # };
    # log { destination(d_file4); };
    # input logs:
    # Oct 11 22:14:15 host-A app-A: message from host-A and app-A
    # Oct 11 22:14:15 host-A app-B: message from host-A and app-B
    # Oct 11 22:14:15 host-B app-A: message from host-B and app-A
    # Oct 11 22:14:15 host-B app-B: message from host-B and app-B

    config = tc.new_config()

    config.create_global_options(keep_hostname="yes")

    file_source = config.create_file_source(file_name="input.log")
    host_filter = config.create_filter(host="'host-A'")
    program_filter = config.create_filter(program="'app-A'")
    file_destination1 = config.create_file_destination(file_name="output1.log")
    file_destination2 = config.create_file_destination(file_name="output2.log")
    file_destination3 = config.create_file_destination(file_name="output3.log")
    file_destination4 = config.create_file_destination(file_name="output4.log")

    second_level_logpath1 = config.create_inner_logpath(statements=[host_filter, file_destination1])

    second_level_logpath2 = config.create_inner_logpath(statements=[program_filter, file_destination2])

    second_level_logpath3 = config.create_inner_logpath(statements=[file_destination3])

    config.create_logpath(
        statements=[file_source, second_level_logpath1, second_level_logpath2, second_level_logpath3]
    )
    config.create_logpath(statements=[file_destination4])

    bsd_message1 = write_message_with_fields(
        tc, file_source, hostname="host-A", program="app-A", message="message from host-A and app-A"
    )
    bsd_message2 = write_message_with_fields(
        tc, file_source, hostname="host-A", program="app-B", message="message from host-A and app-B"
    )
    bsd_message3 = write_message_with_fields(
        tc, file_source, hostname="host-B", program="app-A", message="message from host-B and app-A"
    )
    bsd_message4 = write_message_with_fields(
        tc, file_source, hostname="host-B", program="app-B", message="message from host-B and app-B"
    )

    expected_bsd_message1 = bsd_message1.remove_priority()
    expected_bsd_message2 = bsd_message2.remove_priority()
    expected_bsd_message3 = bsd_message3.remove_priority()
    expected_bsd_message4 = bsd_message4.remove_priority()

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)

    dest1_logs = file_destination1.read_logs(counter=2)
    # host("host-A") matches on first and second messages
    assert tc.format_as_bsd(expected_bsd_message1) in dest1_logs
    assert tc.format_as_bsd(expected_bsd_message2) in dest1_logs

    dest2_logs = file_destination2.read_logs(counter=2)
    # program("app-A") matches on first and third messages
    assert tc.format_as_bsd(expected_bsd_message1) in dest2_logs
    assert tc.format_as_bsd(expected_bsd_message3) in dest2_logs

    dest3_logs = file_destination3.read_logs(counter=4)
    # every message should arrived into destination3
    # there is no filter() on this logpath
    assert tc.format_as_bsd(expected_bsd_message1) in dest3_logs
    assert tc.format_as_bsd(expected_bsd_message2) in dest3_logs
    assert tc.format_as_bsd(expected_bsd_message3) in dest3_logs
    assert tc.format_as_bsd(expected_bsd_message4) in dest3_logs

    # no messages should arrived into destination4,
    # no source() or flags(catch-all) is added
    assert file_destination4.get_path().exists() is False
