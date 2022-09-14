#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 One Identity
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

LOG_MSG_SIZE = 65536
INPUT_MESSAGES = "1" * (LOG_MSG_SIZE + 12) + "\n<167>2022-08-21T00:07:06.7"


def test_syslog_parser_timestamp_spinning(config, syslog_ng, port_allocator):
    config.update_global_options(log_msg_size=LOG_MSG_SIZE)
    network_source = config.create_network_source(ip="localhost", port=port_allocator())
    config.create_logpath(statements=[network_source])

    syslog_ng.start(config)

    network_source.write_log(INPUT_MESSAGES)
    syslog_ng.stop()
    assert not syslog_ng.is_process_running()
