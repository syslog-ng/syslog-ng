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
import pytest

from src.executors.command_executor import CommandExecutor


@pytest.mark.parametrize(
    "command, expected_stdout, expected_stderr, expected_exit_code",
    [(["grep", "a", "a"], "", "grep: a: No such file or directory\n", 2), (["echo", "a"], "a\n", "", 0)],
)
def test_execute_command(tmpdir, command, expected_stdout, expected_stderr, expected_exit_code):
    stdout_file = tmpdir.join("stdout.log")
    stderr_file = tmpdir.join("stderr.log")
    command_executor = CommandExecutor()
    result = command_executor.run(command, stdout_file, stderr_file)
    assert result["stdout"] == expected_stdout
    assert result["stderr"] == expected_stderr
    assert result["exit_code"] == expected_exit_code
