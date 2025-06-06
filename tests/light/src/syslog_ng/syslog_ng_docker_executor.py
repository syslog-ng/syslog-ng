#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Axoflow
# Copyright (c) 2025 Attila Szakacs <attila.szakacs@axoflow.com>
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
import os
import typing
from copy import copy
from pathlib import Path
from subprocess import Popen

from src.executors.process_executor import ProcessExecutor
from src.syslog_ng.syslog_ng_executor import SyslogNgExecutor
from src.syslog_ng.syslog_ng_executor import SyslogNgStartParams


class SyslogNgDockerExecutor(SyslogNgExecutor):
    def __init__(self, container_name: str, image_name: str) -> None:
        self.__process_executor = ProcessExecutor()
        self.__container_name = container_name
        self.__image_name = image_name

    def run_process(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        start_params = copy(start_params)
        start_params.control_socket_path = Path("/tmp/syslog-ng.ctl")

        paths = {
            Path.cwd().absolute(),
            Path(start_params.config_path).parent.absolute(),
            Path(start_params.persist_path).parent.absolute(),
            Path(start_params.pid_path).parent.absolute(),
        }

        command = ["docker", "run", "--rm", "-i"]
        command += ["--name", self.__container_name]
        command += ["--workdir", str(Path.cwd().absolute())]
        command += ["--user", f"{os.getuid()}:{os.getgid()}"]
        command += ["-e", f"PUID={os.getuid()}", "-e", f"PGID={os.getgid()}"]
        command += ["--network", "host"]

        for path in paths:
            command += ["-v", f"{path}:{path}"]

        command += [self.__image_name]
        command += start_params.format()

        return self.__process_executor.start(
            command=command,
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
        raise NotImplementedError()

    def run_process_with_gdb(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        raise NotImplementedError()

    def run_process_with_strace(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
        strace_output_path: Path,
    ) -> Popen:
        raise NotImplementedError()

    def get_backtrace_from_core(
        self,
        core_file_path: Path,
        stderr_path: Path,
        stdout_path: Path,
    ) -> typing.Dict[str, typing.Any]:
        raise NotImplementedError()
