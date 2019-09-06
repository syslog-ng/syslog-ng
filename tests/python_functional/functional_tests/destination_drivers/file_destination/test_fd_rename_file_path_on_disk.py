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
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters


def test_fd_rename_file_path_on_disk(config, syslog_ng, syslog_ng_ctl, generate_msgs_for_dummy_src_and_file_dst):
    dummy_source = config.create_dummy_source()

    original_output_path = Path(tc_parameters.WORKING_DIR, "output.log")
    config.update_global_options(keep_hostname="yes")
    file_destination_original = config.create_file_destination(file_name=str(original_output_path))
    file_destination_after_rename = config.create_file_destination(file_name=str(original_output_path))

    config.create_logpath(statements=[dummy_source, file_destination_original])

    input_messages, expected_messages = generate_msgs_for_dummy_src_and_file_dst

    dummy_source.write_log(input_messages[1])
    syslog_ng.start(config)
    assert file_destination_original.read_log() == expected_messages[1]

    saved_output_path = Path(tc_parameters.WORKING_DIR, "output.log.saved")
    original_output_path.rename(saved_output_path)

    assert saved_output_path.exists()
    assert not original_output_path.exists()

    syslog_ng_ctl.reopen()
    dummy_source.write_log(input_messages[2])

    assert file_destination_after_rename.read_log() == expected_messages[2]
    assert original_output_path.exists()
