#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Alexander Zikulnig
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
import glob
import math
import os
from time import sleep


def test_logrotate_all_messages_received(config, syslog_ng):
    file_name = "logfile-messages-received.log"
    max_size = 10000
    max_rotations = 5
    epsilon = max_size / 10

    message = "message-text"
    msg_per_file = math.floor(max_size / (len(message.encode('utf-8')) + 1))
    counter = msg_per_file * (max_rotations) + math.floor(msg_per_file / 2)

    generator_source = config.create_example_msg_generator_source(num=counter, freq=0.0001, template=config.stringify(message))
    file_destination = config.create_file_destination(file_name=file_name, template="\"${MSG}\\n\"", logrotate="enable(yes), rotations(" + str(max_rotations) + "), size(" + str(max_size) + ")")

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)

    # wait until all messages have been written
    sleep(5)

    # verify that all logfiles have been created
    log_file_list = glob.glob(file_name + "*")
    num_log_files = len(log_file_list)
    assert (num_log_files == max_rotations + 1)

    # verify that all messages were received
    total_log_count = 0
    for file in log_file_list:
        f = open(file)
        total_log_count += sum(1 for _ in f)
        f.close()

    assert (total_log_count == counter)

    # verify the size of all created logfiles (smaller than max_size + some epsilon of 10 percent)
    for file in log_file_list:
        size = os.path.getsize(file)
        assert (size < max_size + epsilon)
