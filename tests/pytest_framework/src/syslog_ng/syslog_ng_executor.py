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

from src.executors.command_executor import CommandExecutor
from src.executors.process_executor import ProcessExecutor
from src.common.random_id import RandomId


class SyslogNgExecutor(object):
    def __init__(self, logger_factory, instance_paths):
        self.__instance_paths = instance_paths
        self.__process_executor = ProcessExecutor(logger_factory)
        self.__command_executor = CommandExecutor(logger_factory)

    def run_process(self):
        return self.__process_executor.start(
            command=self.__construct_syslog_ng_process(),
            stdout_path=self.__instance_paths.get_stdout_path(),
            stderr_path=self.__instance_paths.get_stderr_path(),
        )

    def run_command(self, command_short_name, command):
        return self.__command_executor.run(
            command=self.__construct_syslog_ng_command(command),
            stdout_path=self.__instance_paths.get_stdout_path_with_postfix(postfix=command_short_name),
            stderr_path=self.__instance_paths.get_stderr_path_with_postfix(postfix=command_short_name),
        )

    def get_backtrace_from_core(self, core_file):
        gdb_command_args = [
            "gdb",
            "-ex",
            "bt full",
            "--batch",
            self.__instance_paths.get_syslog_ng_bin(),
            "--core",
            core_file,
        ]
        core_postfix = "gdb_core_{}".format(RandomId(use_static_seed=False).get_unique_id())
        return self.__command_executor.run(
            command=gdb_command_args,
            stdout_path=self.__instance_paths.get_stdout_path_with_postfix(postfix=core_postfix),
            stderr_path=self.__instance_paths.get_stderr_path_with_postfix(postfix=core_postfix),
        )

    def __construct_syslog_ng_process(
        self,
        stderr=True,
        debug=True,
        trace=True,
        verbose=True,
        startup_debug=True,
        no_caps=True,
        config_path=None,
        persist_path=None,
        pid_path=None,
        control_socket_path=None,
    ):
        syslog_ng_process_args = [self.__instance_paths.get_syslog_ng_bin()]
        syslog_ng_process_args += ["--foreground", "--enable-core"]
        if stderr:
            syslog_ng_process_args += ["--stderr"]
        if debug:
            syslog_ng_process_args += ["--debug"]
        if trace:
            syslog_ng_process_args += ["--trace"]
        if verbose:
            syslog_ng_process_args += ["--verbose"]
        if startup_debug:
            syslog_ng_process_args += ["--startup-debug"]
        if no_caps:
            syslog_ng_process_args += ["--no-caps"]
        if config_path is None:
            config_path = self.__instance_paths.get_config_path()
        syslog_ng_process_args += ["--cfgfile={}".format(config_path)]
        if persist_path is None:
            persist_path = self.__instance_paths.get_persist_path()
        syslog_ng_process_args += ["--persist-file={}".format(persist_path)]
        if pid_path is None:
            pid_path = self.__instance_paths.get_pid_path()
        syslog_ng_process_args += ["--pidfile={}".format(pid_path)]
        if control_socket_path is None:
            control_socket_path = self.__instance_paths.get_control_socket_path()
        syslog_ng_process_args += ["--control={}".format(control_socket_path)]
        return syslog_ng_process_args

    def __construct_syslog_ng_command(self, command):
        syslog_ng_command_args = [self.__instance_paths.get_syslog_ng_bin()]
        syslog_ng_command_args += command
        return syslog_ng_command_args
