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


from src.message_builder.log_message import LogMessage


def write_msg_with_fields(file_source, bsd_formatter, hostname, program):
    log_message = LogMessage().hostname(hostname).program(program)
    input_message = bsd_formatter.format_message(log_message)
    expected_message = bsd_formatter.format_message(log_message.remove_priority())
    file_source.write_log(input_message)
    return expected_message


def test_multiple_flags(config, syslog_ng, bsd_formatter):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { filter{host("host-A")}; destination(d_file1); flags(final); };
    #     log { filter{program("app-A")}; destination(d_file2); };
    #     log { destination(d_file3); flags(fallback);};
    # };
    # log { destination(d_file4); flags(catch-all);};
    # input logs:
    # Oct 11 22:14:15 host-A app-A: message from host-A and app-A
    # Oct 11 22:14:15 host-A app-B: message from host-A and app-B
    # Oct 11 22:14:15 host-B app-A: message from host-B and app-A
    # Oct 11 22:14:15 host-B app-B: message from host-B and app-B

    config.create_global_options(keep_hostname="yes")

    file_source = config.create_file_source(file_name="input.log")
    host_filter = config.create_filter(host="'host-A'")
    program_filter = config.create_filter(program="'app-A'")
    file_destination1 = config.create_file_destination(file_name="output1.log")
    file_destination2 = config.create_file_destination(file_name="output2.log")
    file_destination3 = config.create_file_destination(file_name="output3.log")
    file_destination4 = config.create_file_destination(file_name="output4.log")

    second_level_logpath1 = config.create_inner_logpath(statements=[host_filter, file_destination1], flags="final")
    second_level_logpath2 = config.create_inner_logpath(statements=[program_filter, file_destination2])
    second_level_logpath3 = config.create_inner_logpath(statements=[file_destination3], flags="fallback")

    config.create_logpath(
        statements=[file_source, second_level_logpath1, second_level_logpath2, second_level_logpath3]
    )
    config.create_logpath(statements=[file_destination4], flags="catch-all")

    expected_message1 = write_msg_with_fields(file_source, bsd_formatter, "host-A", "app-A")
    expected_message2 = write_msg_with_fields(file_source, bsd_formatter, "host-A", "app-B")
    expected_message3 = write_msg_with_fields(file_source, bsd_formatter, "host-B", "app-A")
    expected_message4 = write_msg_with_fields(file_source, bsd_formatter, "host-B", "app-B")

    syslog_ng.start(config)

    dest1_logs = file_destination1.read_logs(counter=2)
    # host("host-A") matches on first and second messages
    assert expected_message1 in dest1_logs
    assert expected_message2 in dest1_logs

    dest2_logs = file_destination2.read_logs(counter=1)
    # program("app-A") matches on first and third messages, but flags(final)
    # in previous logpath() say that first message can not arrived here
    assert expected_message3 in dest2_logs

    dest3_logs = file_destination3.read_logs(counter=1)
    # only fourth message should arrive here, because of
    # flags(fallback), only this messages was not matched before
    assert expected_message4 in dest3_logs

    dest4_logs = file_destination4.read_logs(counter=4)
    # every message should arrived into destination4
    # there is a flags(catch-all)
    assert expected_message1 in dest4_logs
    assert expected_message2 in dest4_logs
    assert expected_message3 in dest4_logs
    assert expected_message4 in dest4_logs
