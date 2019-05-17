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


def write_msg_with_fields(file_source, bsd_formatter, hostname):
    log_message = LogMessage().hostname(hostname)
    input_message = bsd_formatter.format_message(log_message)
    expected_message = bsd_formatter.format_message(log_message.remove_priority())
    file_source.write_log(input_message)
    return expected_message


def test_flags_fallback(config, syslog_ng, bsd_formatter):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { filter{host("host-A")}; destination(d_file1); };
    #     log { destination(d_file2); flags(fallback);};
    # };
    # input logs:
    # Oct 11 22:14:15 host-A testprogram: message from host-A
    # Oct 11 22:14:15 host-B testprogram: message from host-B

    config.create_global_options(keep_hostname="yes")

    file_source = config.create_file_source(file_name="input.log")
    host_filter = config.create_filter(host="'host-A'")
    file_destination1 = config.create_file_destination(file_name="output1.log")
    file_destination2 = config.create_file_destination(file_name="output2.log")

    inner_logpath1 = config.create_inner_logpath(statements=[host_filter, file_destination1])
    inner_logpath2 = config.create_inner_logpath(statements=[file_destination2], flags="fallback")

    config.create_logpath(statements=[file_source, inner_logpath1, inner_logpath2])

    expected_message1 = write_msg_with_fields(file_source, bsd_formatter, "host-A")
    expected_message2 = write_msg_with_fields(file_source, bsd_formatter, "host-B")

    syslog_ng.start(config)

    dest1_logs = file_destination1.read_log()
    # host("host-A") matches on first and second messages
    assert expected_message1 in dest1_logs

    dest2_logs = file_destination2.read_log()
    # only third message should arrive here, because of
    # flags(fallback), only this messages was not matched before
    assert expected_message2 in dest2_logs
