#!/usr/bin/env python
#############################################################################
# Copyright (c) 2021 One Identity
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
from src.message_builder.bsd_format import BSDFormat
from src.message_builder.log_message import LogMessage


def test_filter_multiple_reference(config, syslog_ng):
    config.update_global_options(keep_hostname="yes")

    file_source = config.create_file_source(file_name="input.log")
    file_src_group = config.create_statement_group(file_source)
    negated_filter = config.create_statement_group(config.create_filter("not (program('noprog') and message('nomsg'))"))
    file_destination1 = config.create_file_destination(file_name="output1.log")
    file_destination2 = config.create_file_destination(file_name="output2.log")

    config.create_logpath(statements=[file_src_group, negated_filter, file_destination1])
    config.create_logpath(statements=[file_src_group, negated_filter, file_destination2])

    log_msg = LogMessage().program("PROGRAM").message("MESSAGE")
    bsd_msg = BSDFormat.format_message(log_msg.remove_priority())

    file_source.write_log(bsd_msg)

    syslog_ng.start(config)

    dest1_logs = file_destination1.read_logs(counter=1)
    assert bsd_msg in dest1_logs

    dest2_logs = file_destination2.read_logs(counter=1)
    assert bsd_msg in dest2_logs
