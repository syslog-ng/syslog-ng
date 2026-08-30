#############################################################################
# Copyright (c) 2025 One Identity
# Copyright (c) 2025 Narkhov Evgeny <evgenynarkhov2@gmail.com>
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
import re
from pathlib import Path

import pytest

from src.bindings.python.python_module import DummyPythonModule
from src.syslog_ng.syslog_ng import SyslogNg
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig


class TestPythonInternalLogging:
    @pytest.fixture(autouse=True)
    def current_module(self, python_module: DummyPythonModule) -> DummyPythonModule:
        python_module.add_file(Path(__file__).parent / "parser.py")
        return python_module

    def __create_common_log_path(self, config: SyslogNgConfig, current_module: DummyPythonModule):
        source = config.create_example_msg_generator_source(num=1, template=config.stringify("hello, world!"))
        parser = config.create_python_parser(**{"class": "%s.parser.ParserThatLogsToInternal" % current_module.module_name})
        config.create_logpath(statements=[source, parser])

    def __wait_and_extract_effective_log_level(self, syslog_ng: SyslogNg) -> int:
        level_line = syslog_ng.wait_for_message_in_console_log("the log level is")[0]
        matched = re.match(r".* (\d+);", level_line)

        assert matched is not None
        return int(matched.group(1))

    def test_trace_logs_get_to_stderr(self, config: SyslogNgConfig, syslog_ng: SyslogNg, current_module: DummyPythonModule):
        """ Test that internal python logger logs trace messages. """

        self.__create_common_log_path(config, current_module)
        syslog_ng.start_params.trace = True
        syslog_ng.start_params.debug = True

        syslog_ng.start(config)
        # We expect one message for each log level present: TRACE, DEBUG, INFO, WARNING and ERROR.
        # Last one comes from trace() method.
        syslog_ng.wait_for_messages_in_console_log([
            "hey, this is parser-that-logs-to-internal, here is the message: hello, world!" for _ in range(5)
        ] + [
            "hey again, here is the bound trace method of internal logger!",
        ])

        assert self.__wait_and_extract_effective_log_level(syslog_ng) < logging.DEBUG

    def test_debug_logs_get_to_stderr(self, config: SyslogNgConfig, syslog_ng: SyslogNg, current_module: DummyPythonModule):
        """ Test that internal python logger filters trace messages with effective log level. """

        self.__create_common_log_path(config, current_module)
        syslog_ng.start_params.trace = False
        syslog_ng.start_params.debug = True

        syslog_ng.start(config)
        # Now, TRACE is left out.
        syslog_ng.wait_for_messages_in_console_log([
            "hey, this is parser-that-logs-to-internal, here is the message: hello, world!" for _ in range(4)
        ])

        assert self.__wait_and_extract_effective_log_level(syslog_ng) < logging.INFO

    def test_info_plus_logs_get_to_stderr(self, config: SyslogNgConfig, syslog_ng: SyslogNg, current_module: DummyPythonModule):
        """ Test that internal python logger filters trace messages with effective log level. """

        self.__create_common_log_path(config, current_module)
        syslog_ng.start_params.trace = False
        syslog_ng.start_params.debug = False
        # This flag turns off logging to standard error if debug is not present, so turn this off, too.
        syslog_ng.start_params.startup_debug = False

        syslog_ng.start(config)
        # DEBUG is also gone now.
        syslog_ng.wait_for_messages_in_console_log([
            "hey, this is parser-that-logs-to-internal, here is the message: hello, world!" for _ in range(3)
        ])

        assert self.__wait_and_extract_effective_log_level(syslog_ng) == logging.INFO
