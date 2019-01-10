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


def write_message_with_fields(tc, file_source, hostname, message):
    bsd_message = tc.new_log_message().hostname(hostname).message(message)
    bsd_log = tc.format_as_bsd(bsd_message)
    file_source.write_log(bsd_log)
    return bsd_message


def test_flags_final(tc):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { filter{host("host-A")}; destination(d_file1); flags(final); };
    #     log { destination(d_file2); };
    # };
    # input logs:
    # Oct 11 22:14:15 host-A testprogram: message from host-A
    # Oct 11 22:14:15 host-B testprogram: message from host-B

    config = tc.new_config()

    config.create_global_options(keep_hostname="yes")

    file_source = config.create_file_source(file_name="input.log")
    source_group = config.create_statement_group(file_source)

    host_filter = config.create_filter(host="'host-A'")
    filter_group1 = config.create_statement_group(host_filter)

    file_destination1 = config.create_file_destination(file_name="output1.log")
    destination_group1 = config.create_statement_group(file_destination1)

    file_destination2 = config.create_file_destination(file_name="output2.log")
    destination_group2 = config.create_statement_group(file_destination2)

    inner_logpath1 = config.create_inner_logpath(statements=[filter_group1, destination_group1], flags="final")

    inner_logpath2 = config.create_inner_logpath(statements=[destination_group2])

    config.create_logpath(statements=[source_group, inner_logpath1, inner_logpath2])

    bsd_message1 = write_message_with_fields(tc, file_source, hostname="host-A", message="message from host-A")
    bsd_message2 = write_message_with_fields(tc, file_source, hostname="host-B", message="message from host-B")

    expected_bsd_message1 = bsd_message1.remove_priority()
    expected_bsd_message2 = bsd_message2.remove_priority()

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)

    dest1_logs = file_destination1.read_log()
    # host("host-A") matches on first message
    assert tc.format_as_bsd(expected_bsd_message1) in dest1_logs

    dest2_logs = file_destination2.read_log()
    # second message should arrived only into destination3
    # first message was stopped on first logpath(), because of
    # flags(final)
    assert tc.format_as_bsd(expected_bsd_message2) in dest2_logs
