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
import typing
from copy import copy
from pathlib import Path
from subprocess import Popen

from src.common.blocking import wait_until_false
from src.common.blocking import wait_until_true
from src.common.random_id import get_unique_id
from src.syslog_ng.console_log_reader import ConsoleLogReader
from src.syslog_ng.syslog_ng_executor import SyslogNgExecutor
from src.syslog_ng.syslog_ng_executor import SyslogNgStartParams
from src.syslog_ng.syslog_ng_paths import SyslogNgPaths
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig
from src.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.testcase_parameters.testcase_parameters import TestcaseParameters

logger = logging.getLogger(__name__)


class SyslogNg(object):
    def __init__(
        self,
        syslog_ng_executor: SyslogNgExecutor,
        syslog_ng_ctl: SyslogNgCtl,
        instance_paths: SyslogNgPaths,
        testcase_parameters: TestcaseParameters,
        teardown,
    ) -> None:
        self.instance_paths = instance_paths
        self.start_params = SyslogNgStartParams(
            config_path=self.instance_paths.get_config_path(),
            persist_path=self.instance_paths.get_persist_path(),
            pid_path=self.instance_paths.get_pid_path(),
            control_socket_path=self.instance_paths.get_control_socket_path(),
        )
        self.__external_tool = testcase_parameters.get_external_tool()
        self.__console_log_reader = ConsoleLogReader(self.instance_paths, teardown)
        self.__syslog_ng_ctl = syslog_ng_ctl
        self.__syslog_ng_executor = syslog_ng_executor
        self.__process: typing.Optional[Popen] = None

    def start(self, config: SyslogNgConfig) -> Popen:
        if self.__process:
            raise Exception("syslog-ng has been already started")

        config.write_config(self.instance_paths.get_config_path())

        self.__syntax_check()
        self.__start_syslog_ng()

        logger.info("syslog-ng process has been started with PID: {}\n".format(self.__process.pid))

        return self.__process

    def stop(self, unexpected_messages: typing.List[str] = None) -> None:
        if self.is_process_running():
            saved_pid = self.__process.pid
            # effective stop
            result = self.__syslog_ng_ctl.stop()

            # wait for stop and check stop result
            if result["exit_code"] != 0:
                self.__error_handling("Control socket fails to stop syslog-ng")
            if not wait_until_false(self.is_process_running):
                self.__error_handling("syslog-ng did not stop")
            if self.start_params.stderr and self.start_params.debug and self.start_params.verbose:
                if not self.__console_log_reader.wait_for_stop_message():
                    self.__error_handling("Stop message not arrived")
            self.__console_log_reader.check_for_unexpected_messages(unexpected_messages)
            if self.__external_tool == "valgrind":
                self.__console_log_reader.handle_valgrind_log(Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_valgrind_output"))
            logger.info("syslog-ng process has been stopped with PID: {}\n".format(saved_pid))
        self.__process = None

    def reload(self, config: SyslogNgConfig) -> None:
        config.write_config(self.instance_paths.get_config_path())

        # effective reload
        result = self.__syslog_ng_ctl.reload()

        # wait for reload and check reload result
        if result["exit_code"] != 0:
            self.__error_handling("Control socket fails to reload syslog-ng")
        if not self.__wait_for_control_socket_alive():
            self.__error_handling("Control socket not alive")
        if self.start_params.stderr and self.start_params.debug and self.start_params.verbose:
            if not self.__console_log_reader.wait_for_reload_message():
                self.__error_handling("Reload message not arrived")
        logger.info("syslog-ng process has been reloaded with PID: {}\n".format(self.__process.pid))

    def restart(self, config: SyslogNgConfig) -> None:
        self.stop()
        self.start(config)

    def get_config_version(self) -> str:
        stdout_path = Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_version_stdout")
        stderr_path = Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_version_stderr")

        start_params = copy(self.start_params)
        start_params.version = True

        process = self.__syslog_ng_executor.run_process(
            start_params=start_params,
            stderr_path=stderr_path,
            stdout_path=stdout_path,
        )
        returncode = process.wait()
        if returncode != 0:
            raise Exception(f"Cannot get syslog-ng config version. Process returned with {returncode}. See {stderr_path.absolute()} for details")

        for version_output_line in stdout_path.read_text().splitlines():
            if "Config version:" in version_output_line:
                return version_output_line.split()[2]
        raise Exception("Can not parse 'Config version' from 'syslog-ng --version'")

    def is_process_running(self) -> bool:
        return self.__process and self.__process.poll() is None

    def wait_for_messages_in_console_log(self, expected_messages: typing.List[str]) -> typing.List[str]:
        assert issubclass(type(expected_messages), list)
        return self.__console_log_reader.wait_for_messages_in_console_log(expected_messages)

    def wait_for_message_in_console_log(self, expected_message: str) -> typing.List[str]:
        return self.wait_for_messages_in_console_log([expected_message])

    def __syntax_check(self) -> None:
        stdout_path = Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_syntax_only_stdout")
        stderr_path = Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_syntax_only_stderr")

        start_params = copy(self.start_params)
        start_params.syntax_only = True

        process = self.__syslog_ng_executor.run_process(
            start_params=start_params,
            stderr_path=stderr_path,
            stdout_path=stdout_path,
        )
        returncode = process.wait()
        if returncode != 0:
            raise Exception(f"syslog-ng syntax error. See {stderr_path.absolute()} for details")

    def __wait_for_control_socket_alive(self) -> bool:
        def is_alive(s: SyslogNg) -> bool:
            if not s.is_process_running():
                self.__error_handling("syslog-ng is not running")
            return s.__syslog_ng_ctl.is_control_socket_alive()
        return wait_until_true(is_alive, self)

    def __wait_for_start(self) -> None:
        # wait for start and check start result
        if not self.__wait_for_control_socket_alive():
            self.__error_handling("Control socket not alive")
        if not self.__console_log_reader.wait_for_start_message():
            self.__error_handling("Start message not arrived")

    def __start_syslog_ng(self) -> None:
        args = {
            "start_params": self.start_params,
            "stderr_path": self.instance_paths.get_stderr_path(),
            "stdout_path": self.instance_paths.get_stdout_path(),
        }

        if self.__external_tool is None:
            self.__process = self.__syslog_ng_executor.run_process(**args)
        elif self.__external_tool == "gdb":
            self.__process = self.__syslog_ng_executor.run_process_with_gdb(**args)
        elif self.__external_tool == "valgrind":
            self.__process = self.__syslog_ng_executor.run_process_with_valgrind(
                valgrind_output_path=Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_valgrind_output"),
                **args,
            )
        elif self.__external_tool == "strace":
            self.__process = self.__syslog_ng_executor.run_process_with_strace(
                strace_output_path=Path(f"syslog_ng_{self.instance_paths.get_instance_name()}_strace_output"),
                **args,
            )
        else:
            raise Exception(f"Unknown external tool: {self.__external_tool}")

        if self.start_params.stderr and self.start_params.debug and self.start_params.verbose:
            self.__wait_for_start()

    def __error_handling(self, error_message: str) -> typing.NoReturn:
        self.__console_log_reader.dump_stderr()
        self.__handle_core_file()
        raise Exception(error_message)

    def __handle_core_file(self) -> None:
        if not self.is_process_running():
            core_file_found = False
            for core_file in Path(".").glob("*core*"):
                core_file_found = True
                self.__process = None

                core_postfix = "gdb_core_{}".format(get_unique_id())
                stderr_path = self.instance_paths.get_stderr_path_with_postfix(core_postfix)
                stdout_path = self.instance_paths.get_stdout_path_with_postfix(core_postfix)

                self.__syslog_ng_executor.get_backtrace_from_core(
                    core_file,
                    stderr_path,
                    stdout_path,
                )
                core_file.replace(Path(core_file))
            if core_file_found:
                raise Exception("syslog-ng core file was found and processed")
            if self.__process.returncode in [-6, -9, -11]:
                ret_code = self.__process.returncode
                self.__process = None
                raise Exception("syslog-ng process crashed with signal {}".format(ret_code))
