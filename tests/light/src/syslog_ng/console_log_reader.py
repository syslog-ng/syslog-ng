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

from src.common.file import File

logger = logging.getLogger(__name__)


class ConsoleLogReader(object):
    def __init__(self, instance_paths):
        self.__stderr_path = instance_paths.get_stderr_path()
        self.__stderr_file = File(self.__stderr_path)

    def wait_for_start_message(self):
        syslog_ng_start_message = ["syslog-ng starting up;"]
        return self.wait_for_messages_in_console_log(syslog_ng_start_message)

    def wait_for_stop_message(self):
        syslog_ng_stop_message = ["syslog-ng shutting down"]
        return self.wait_for_messages_in_console_log(syslog_ng_stop_message)

    def wait_for_reload_message(self):
        syslog_ng_reload_messages = [
            "New configuration initialized",
            "Configuration reload request received, reloading configuration",
            "Configuration reload finished",
        ]
        return self.wait_for_messages_in_console_log(syslog_ng_reload_messages)

    def wait_for_message_in_console_log(self, expected_message):
        return self.wait_for_messages_in_console_log(self, [expected_message])

    def wait_for_messages_in_console_log(self, expected_messages):
        if not self.__stderr_file.is_opened():
            self.__stderr_file.wait_for_creation()
            self.__stderr_file.open("r")
        return self.__stderr_file.wait_for_lines(expected_messages, timeout=5)

    def check_for_unexpected_messages(self, unexpected_messages=None):
        unexpected_patterns = ["Plugin module not found", "assertion"]
        if unexpected_messages is not None:
            unexpected_patterns.extend(unexpected_messages)

        stderr = self.__read_all_from_stderr_file()

        for unexpected_pattern in unexpected_patterns:
            for console_log_message in stderr.split("\n"):
                if unexpected_pattern in console_log_message:
                    logger.error("Found unexpected message in console log: {}".format(console_log_message))
                    raise Exception("Unexpected error log in console", console_log_message)

    def dump_stderr(self, last_n_lines=10):
        stderr = self.__read_all_from_stderr_file()
        logger.error("\n".join(stderr.split("\n")[-last_n_lines:]))

    def __read_all_from_stderr_file(self):
        stderr_file_from_the_beginning = File(self.__stderr_path)
        stderr_file_from_the_beginning.open("r")
        stderr = stderr_file_from_the_beginning.read()
        stderr_file_from_the_beginning.close()
        return stderr

    @staticmethod
    def handle_valgrind_log(valgrind_log_path):
        with open(valgrind_log_path, "r") as valgrind_log:
            valgrind_content = valgrind_log.read()
            assert "Invalid read" not in valgrind_content
            assert "Invalid write" not in valgrind_content
            assert "blocks are definitely lost in loss record" not in valgrind_content
            assert "Uninitialised value was created by a heap allocation" not in valgrind_content
