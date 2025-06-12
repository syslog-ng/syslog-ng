#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Axoflow
# Copyright (c) 2025 Attila Szakacs <attila.szakacs@axoflow.com>
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
import shlex
import typing
from pathlib import Path
from subprocess import Popen

from src.executors.command_executor import CommandExecutor
from src.executors.process_executor import ProcessExecutor
from src.syslog_ng.syslog_ng_executor import SyslogNgExecutor
from src.syslog_ng.syslog_ng_executor import SyslogNgStartParams


class SyslogNgLocalExecutor(SyslogNgExecutor):
    def __init__(
        self,
        syslog_ng_binary_path: Path,
    ) -> None:
        self.__process_executor = ProcessExecutor()
        self.__command_executor = CommandExecutor()
        self.__syslog_ng_binary_path = syslog_ng_binary_path

    def run_process(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        return self.__process_executor.start(
            command=self.__construct_syslog_ng_command(start_params),
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )

    def run_process_with_valgrind(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
        valgrind_output_path: Path,
    ) -> Popen:
        valgrind_command_args = [
            "valgrind",
            "--show-leak-kinds=all",
            "--track-origins=yes",
            "--tool=memcheck",
            "--leak-check=full",
            "--keep-stacktraces=alloc-and-free",
            "--read-var-info=yes",
            "--error-limit=no",
            "--num-callers=40",
            "--verbose",
            f"--log-file={str(valgrind_output_path)}",
        ]
        full_command_args = valgrind_command_args + self.__construct_syslog_ng_command(start_params)
        return self.__process_executor.start(
            command=full_command_args,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )

    def run_process_with_gdb(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        gdb_command_args = [
            "gdb",
            "-ex",
            f"r {shlex.join(start_params.format())} > {str(stdout_path)} 2> {str(stderr_path)}",
            str(self.__syslog_ng_binary_path()),
        ]
        return self.__process_executor.start(
            command=["xterm", "-fa", "Monospace", "-fs", "18", "-e", shlex.join(gdb_command_args)],
            stdout_path="/dev/null",
            stderr_path="/dev/null",
        )

    def run_process_with_strace(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
        strace_output_path: Path,
    ) -> Popen:
        strace_command_args = [
            "strace",
            "-s",
            "4096",
            "-tt",
            "-T",
            "-ff",
            "-o",
            str(strace_output_path),
        ]
        full_command_args = strace_command_args + self.__construct_syslog_ng_command(start_params)
        return self.__process_executor.start(
            command=full_command_args,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )

    def get_backtrace_from_core(
        self,
        core_file_path: Path,
        stderr_path: Path,
        stdout_path: Path,
    ) -> typing.Dict[str, typing.Any]:
        gdb_command_args = [
            "gdb",
            "-ex",
            "bt full",
            "--batch",
            str(self.__syslog_ng_binary_path),
            "--core",
            str(core_file_path),
        ]
        return self.__command_executor.run(
            command=gdb_command_args,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )

    def __construct_syslog_ng_command(
        self,
        start_params: typing.Optional[SyslogNgStartParams] = None,
    ) -> typing.List[str]:
        return [str(self.__syslog_ng_binary_path)] + start_params.format()
