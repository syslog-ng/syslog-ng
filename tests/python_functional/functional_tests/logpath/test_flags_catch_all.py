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


def test_flags_catch_all(config, syslog_ng, log_message, bsd_formatter):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { destination(d_file1); };
    # };
    # log { destination(d_file2); flags(catch-all);};
    # input logs:
    # Oct 11 22:14:15 host-A testprogram: message from host-A

    file_source = config.file_source(file_name="input.log")
    file_destination = config.file_destination(file_name="output.log")
    catch_all_destination = config.file_destination(file_name="catchall_output.log")

    inner_logpath = config.create_inner_logpath(statements=[file_destination])

    config.create_logpath(statements=[file_source, inner_logpath])
    config.create_logpath(statements=[catch_all_destination], flags="catch-all")

    input_message = bsd_formatter.format_message(log_message)
    expected_message = bsd_formatter.format_message(log_message.remove_priority())
    file_source.write_log(input_message)

    syslog_ng.start(config)

    destination_log = file_destination.read_log()
    # message should arrived into destination1
    assert expected_message in destination_log

    catch_all_destination_log = catch_all_destination.read_log()
    # message should arrived into catch_all_destination
    # there is a flags(catch-all)
    assert expected_message in catch_all_destination_log
