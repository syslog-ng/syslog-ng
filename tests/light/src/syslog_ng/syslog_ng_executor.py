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
import typing
from abc import ABC
from abc import abstractmethod
from dataclasses import dataclass
from pathlib import Path
from subprocess import Popen


@dataclass
class SyslogNgStartParams:
    foreground: bool = True
    enable_core: bool = True
    stderr: bool = True
    debug: bool = True
    trace: bool = True
    verbose: bool = True
    startup_debug: bool = True
    no_caps: bool = True
    syntax_only: bool = False
    version: bool = False
    config_path: typing.Optional[Path] = None
    persist_path: typing.Optional[Path] = None
    pid_path: typing.Optional[Path] = None
    control_socket_path: typing.Optional[Path] = None
    preprocess_into: typing.Optional[Path] = None

    def format(self) -> typing.List[str]:
        params = []
        if self.foreground:
            params += ["--foreground"]
        if self.enable_core:
            params += ["--enable-core"]
        if self.stderr:
            params += ["--stderr"]
        if self.debug:
            params += ["--debug"]
        if self.trace:
            params += ["--trace"]
        if self.verbose:
            params += ["--verbose"]
        if self.startup_debug:
            params += ["--startup-debug"]
        if self.no_caps:
            params += ["--no-caps"]
        if self.syntax_only:
            params += ["--syntax-only"]
        if self.version:
            params += ["--version"]
        if self.config_path is not None:
            params += [f"--cfgfile={self.config_path}"]
        if self.persist_path is not None:
            params += [f"--persist-file={self.persist_path}"]
        if self.pid_path is not None:
            params += [f"--pidfile={self.pid_path}"]
        if self.control_socket_path is not None:
            params += [f"--control={self.control_socket_path}"]
        if self.preprocess_into is not None:
            params += [f"--preprocess-into={self.preprocess_into}"]
        return params


class SyslogNgExecutor(ABC):
    @abstractmethod
    def run_process(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        pass

    @abstractmethod
    def run_process_with_valgrind(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
        valgrind_output_path: Path,
    ) -> Popen:
        pass

    @abstractmethod
    def run_process_with_gdb(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
    ) -> Popen:
        pass

    @abstractmethod
    def run_process_with_strace(
        self,
        start_params: SyslogNgStartParams,
        stderr_path: Path,
        stdout_path: Path,
        strace_output_path: Path,
    ) -> Popen:
        pass

    @abstractmethod
    def get_backtrace_from_core(
        self,
        core_file_path: Path,
        stderr_path: Path,
        stdout_path: Path,
    ) -> typing.Dict[str, typing.Any]:
        pass
