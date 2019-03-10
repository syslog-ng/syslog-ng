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

from src.driver_io.file.file_io import FileIO
from src.message_reader.message_reader import MessageReader
from src.message_reader.message_reader import READ_ALL_MESSAGES
from src.message_reader.single_line_parser import SingleLineParser

logger = logging.getLogger(__name__)


class ConsoleLogReader(object):
    def __init__(self, instance_paths):
        self.__stderr_io = FileIO(instance_paths.get_stderr_path())
        self.__message_reader = MessageReader(self.__stderr_io.read, SingleLineParser())

    def wait_for_start_message(self):
        syslog_ng_start_message = ["syslog-ng starting up;"]
        return self.__wait_for_messages_in_console_log(syslog_ng_start_message)

    def wait_for_stop_message(self):
        syslog_ng_stop_message = ["syslog-ng shutting down"]
        return self.__wait_for_messages_in_console_log(syslog_ng_stop_message)

    def wait_for_reload_message(self):
        syslog_ng_reload_messages = [
            "New configuration initialized",
            "Configuration reload request received, reloading configuration",
            "Configuration reload finished",
        ]
        return self.__wait_for_messages_in_console_log(syslog_ng_reload_messages)

    def __wait_for_messages_in_console_log(self, expected_messages):
        if not self.__stderr_io.wait_for_creation():
            raise Exception

        console_log_messages = self.__message_reader.pop_messages(counter=READ_ALL_MESSAGES)
        console_log_content = "".join(console_log_messages)

        result = []
        for expected_message in expected_messages:
            result.append(expected_message in console_log_content)
        return all(result)

    def check_for_unexpected_messages(self, unexpected_messages):
        unexpected_patterns = ["Plugin module not found"]
        console_log_messages = self.__message_reader.peek_messages(counter=READ_ALL_MESSAGES)
        if unexpected_messages is not None:
            unexpected_patterns.append(unexpected_messages)
        for unexpected_pattern in unexpected_patterns:
            for console_log_message in console_log_messages:
                if unexpected_pattern in console_log_message:
                    logger.error("Found unexpected message in console log: {}".format(console_log_message))
                    assert False

    def dump_stderr(self, last_n_lines=10):
        console_log_messages = self.__message_reader.peek_messages(counter=READ_ALL_MESSAGES)
        logger.error("".join(console_log_messages[-last_n_lines:]))

    @staticmethod
    def handle_valgrind_log(valgrind_log_path):
        with open(valgrind_log_path, "r") as valgrind_log:
            valgrind_content = valgrind_log.read()
            assert "Invalid read" not in valgrind_content
            assert "Invalid write" not in valgrind_content
            assert "blocks are definitely lost in loss record" not in valgrind_content
            assert "Uninitialised value was created by a heap allocation" not in valgrind_content
