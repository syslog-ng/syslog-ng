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


def test_flags_catch_all(config, syslog_ng, generate_msgs_for_dummy_src_and_dst):
    # Check the correct output if the logpath is the following
    # log {
    #     source(s_file);
    #     log { destination(d_file1); };
    # };
    # log { destination(d_file2); flags(catch-all);};
    # input logs:
    # Oct 11 22:14:15 host-A testprogram: message from host-A

    dummy_source = config.create_dummy_source()
    inner_destination = config.create_dummy_destination()

    catch_all_destination = config.create_dummy_destination()

    inner_logpath = config.create_inner_logpath(statements=[inner_destination])

    config.create_logpath(statements=[dummy_source, inner_logpath])
    config.create_logpath(statements=[catch_all_destination], flags="catch-all")
    config.update_global_options(keep_hostname="yes")

    input_messages, expected_messages = generate_msgs_for_dummy_src_and_dst

    dummy_source.write_log(input_messages[1])

    syslog_ng.start(config)

    destination_log = inner_destination.read_log()
    # message should arrived into destination1
    assert expected_messages[1] in destination_log

    catch_all_destination_log = catch_all_destination.read_log()
    # message should arrived into catch_all_destination
    # there is a flags(catch-all)
    assert expected_messages[1] in catch_all_destination_log
