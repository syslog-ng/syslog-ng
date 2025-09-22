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


def test_logrotate_rotation_failure(config, syslog_ng):
    src_file_name = "input-rotation-failure.log"
    dest_file_name = "logfile-rotation-failure.log"
    max_size = 1000
    max_rotations = 3

    message = "message-text\n"
    msg_per_file = math.ceil(max_size / len(message.encode('utf-8')))
    counter = msg_per_file * max_rotations

    file_source = config.create_file_source(file_name=src_file_name)
    file_destination = config.create_file_destination(file_name=dest_file_name, logrotate="enable(yes), rotations(" + str(max_rotations) + "), size(" + str(max_size) + ")")

    config.create_logpath(statements=[file_source, file_destination])

    # write messages to src file
    for _ in range(counter):
        file_source.write_log(message)

    syslog_ng.start(config)

    # wait until syslog has written logfiles
    sleep(2)

    # check current number of logfiles
    log_file_list = glob.glob(dest_file_name + "*")
    num_log_files = len(log_file_list)
    assert (num_log_files == max_rotations + 1)

    # simulate rotation error by deleting first rotated logfile
    os.remove(dest_file_name + ".1")

    # write enough log messages to trigger rotation
    for _ in range(msg_per_file):
        file_source.write_log(message)

    # wait until syslog has written logfiles
    sleep(2)

    # rotation should recognise a logfile is missing and not delete other files
    log_file_list = glob.glob(dest_file_name + "*")
    num_log_files = len(log_file_list)
    assert (num_log_files == max_rotations + 1)
