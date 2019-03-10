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
import logging

import psutil

from src.driver_io.file.file import File

logger = logging.getLogger(__name__)

def prepare_std_outputs(file_ref, stdout_path, stderr_path):
    stdout = file_ref(stdout_path)
    stderr = file_ref(stderr_path)
    return stdout, stderr


def prepare_printable_command(command):
    printable_command = ""
    for command_arg in command:
        printable_command += "{} ".format(str(command_arg))
    return printable_command


def prepare_executable_command(command):
    executable_command = []
    for command_arg in command:
        executable_command.append(str(command_arg))
    return executable_command


class CommandExecutor(object):
    def __init__(self):
        self.__file_ref = File
        self.__start_timeout = 10

    def run(self, command, stdout_path, stderr_path):
        printable_command = prepare_printable_command(command)
        executable_command = prepare_executable_command(command)
        stdout, stderr = prepare_std_outputs(self.__file_ref, stdout_path, stderr_path)
        logger.debug("The following command will be executed:\n{}\n".format(printable_command))
        cmd = psutil.Popen(executable_command, stdout=stdout.open_file(mode="w"), stderr=stderr.open_file(mode="w"))
        exit_code = cmd.wait(timeout=self.__start_timeout)

        stdout_content, stderr_content = self.__process_std_outputs(stdout, stderr)
        self.__process_exit_code(printable_command, exit_code, stdout_content, stderr_content)
        return {"exit_code": exit_code, "stdout": stdout_content, "stderr": stderr_content}

    def __process_std_outputs(self, stdout, stderr):
        stdout_content = stdout.open_file("r").read()
        stderr_content = stderr.open_file("r").read()
        return stdout_content, stderr_content

    def __process_exit_code(self, command, exit_code, stdout, stderr):
        exit_code_debug_log = "\nExecuted command: {}\n\
Exit code: {}\n\
Stdout: {}\n\
Stderr: {}\n".format(command.rstrip(), exit_code, stdout.rstrip(), stderr.rstrip())
        logger.debug(exit_code_debug_log)
