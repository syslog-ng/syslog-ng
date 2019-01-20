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

from pathlib2 import Path
from src.syslog_ng.syslog_ng_executor import SyslogNgExecutor
from src.syslog_ng.console_log_reader import ConsoleLogReader
from src.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.common.blocking import wait_until_true, wait_until_false

class SyslogNgCli(object):
    def __init__(self, logger_factory, instance_paths, testcase_parameters):
        self.__instance_paths = instance_paths
        self.__console_log_reader = ConsoleLogReader(logger_factory, instance_paths)
        self.__logger = logger_factory.create_logger("SyslogNgCli")
        self.__syslog_ng_executor = SyslogNgExecutor(logger_factory, instance_paths)
        self.__syslog_ng_ctl = SyslogNgCtl(logger_factory, instance_paths)
        self.__valgrind_usage = testcase_parameters.get_valgrind_usage()
        self.__process = None

    # Application commands
    def get_version(self):
        version_output = self.__syslog_ng_executor.run_command(command_short_name="version", command=["--version"])[
            "stdout"
        ]
        for version_output_line in version_output.splitlines():
            if "Config version:" in version_output_line:
                return version_output_line.split()[2]
        raise Exception("Can not parse 'Config version' from ./syslog-ng --version")

    def __syntax_only(self, config_path=None):
        if config_path is None:
            config_path = self.__instance_paths.get_config_path()
        return self.__syslog_ng_executor.run_command(
            command_short_name="syntax_only", command=["--syntax-only", "--cfgfile={}".format(config_path)]
        )

    def __syntax_check(self):
        result = self.__syntax_only()
        if result["exit_code"] != 0:
            self.__logger.error(result["stderr"])
            raise Exception("syslog-ng can not started")

    def __is_process_running(self):
        return self.__process.poll() == None

    def __wait_for_control_socket_alive(self):
        def is_alive(s):
            if not s.__is_process_running():
                raise Exception("syslog-ng could not start")
            return s.__syslog_ng_ctl.is_control_socket_alive()
        return wait_until_true(is_alive, self)

    def __wait_for_start(self):
        # wait for start and check start result
        if not self.__wait_for_control_socket_alive():
            self.__error_handling()
            raise Exception("Control socket not alive")
        if not self.__console_log_reader.wait_for_start_message():
            self.__error_handling()
            raise Exception("Start message not arrived")

    def __start_syslog_ng(self):
        if self.__valgrind_usage:
            self.__process = self.__syslog_ng_executor.run_process_with_valgrind()
        else:
            self.__process = self.__syslog_ng_executor.run_process()
        self.__wait_for_start()

    # Process commands
    def start(self, config):
        self.__logger.info("Beginning of syslog-ng start")
        config.set_version(self.get_version())
        config.write_config_content()

        self.__syntax_check()
        self.__start_syslog_ng()

        self.__logger.info("End of syslog-ng start")

    def reload(self, config):
        self.__logger.info("Beginning of syslog-ng reload")
        config.set_version(self.get_version())
        config.write_config_content()

        # effective reload
        self.__syslog_ng_ctl.reload()

        # wait for reload and check reload result
        if not self.__wait_for_control_socket_alive():
            self.__error_handling()
            raise Exception("Control socket not alive")
        if not self.__console_log_reader.wait_for_reload_message():
            self.__error_handling()
            raise Exception("Reload message not arrived")
        self.__logger.info("End of syslog-ng reload")

    def stop(self, unexpected_messages=None):
        self.__logger.info("Beginning of syslog-ng stop")
        if self.__process:
            # effective stop
            result = self.__syslog_ng_ctl.stop()

            # wait for stop and check stop result
            if result["exit_code"] != 0:
                self.__error_handling()
            if not wait_until_false(self.__is_process_running):
                self.__error_handling()
                raise Exception("syslog-ng did not stop")
            if not self.__console_log_reader.wait_for_stop_message():
                self.__error_handling()
                raise Exception("Stop message not arrived")
            self.__console_log_reader.check_for_unexpected_messages(unexpected_messages)
            if self.__valgrind_usage:
                self.__console_log_reader.handle_valgrind_log(self.__instance_paths.get_valgrind_log_path())
            self.__process = None
            self.__logger.info("End of syslog-ng stop")

    # Helper functions
    def __error_handling(self):
        self.__console_log_reader.dump_stderr()
        self.__handle_core_file()

    def __handle_core_file(self):
        if self.__process.wait(1) != 0:
            core_file_found = False
            for core_file in Path(".").glob("*core*"):
                core_file_found = True
                self.__process = None
                self.__syslog_ng_executor.get_backtrace_from_core(core_file=str(core_file))
                core_file.replace(Path(self.__instance_paths.get_working_dir(), core_file))
            if core_file_found:
                raise Exception("syslog-ng core file was found and processed")
