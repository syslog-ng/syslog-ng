#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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

from src.executors.process_executor import ProcessExecutor


def test_start_stop_process(tc_unittest):
    process_executor = ProcessExecutor(tc_unittest.get_fake_logger_factory())
    process_command = ["python", "-c", "import time; time.sleep(3)"]
    stdout_file = tc_unittest.get_temp_file()
    stderr_file = tc_unittest.get_temp_file()

    process_executor.start(process_command, stdout_file, stderr_file)
    assert process_executor.process is not None
    assert process_executor.process.pid is not None
