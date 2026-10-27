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
from pathlib import Path

import pytest

from src.syslog_ng.syslog_ng import SyslogNg
from src.syslog_ng_config.statements import ArrowedOptions
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig


class TestPythonParser:
    @pytest.fixture
    def current_module(self, python_module):
        python_module.add_file(Path(__file__).parent / "parser.py")
        return python_module

    def test_parser_copies_and_drops_messages(self, config: SyslogNgConfig, syslog_ng: SyslogNg, current_module):
        """
        Test that python `CopyParser` can read options, successfully `init()` and then copy **MSG** field with `parse()` method.
        """

        bye_source = config.create_example_msg_generator_source(num=1, template=config.stringify("bye, world!"))
        hello_source = config.create_example_msg_generator_source(num=1, template=config.stringify("hello, world!"))

        parser_cls_options = ArrowedOptions({"target_field": "\"FOO\"", "drop_prefix": "\"bye\""})
        # Unfortunately, class is reserved name in python, so I have to do this little trick
        parser = config.create_python_parser(**{"class": "%s.parser.CopyParser" % current_module.module_name, "options": parser_cls_options})
        destination = config.create_file_destination(file_name="output.log", template=config.stringify("$FOO\n"))
        config.create_logpath(statements=[bye_source, hello_source, parser, destination])

        syslog_ng.start(config)

        logs = destination.read_log().strip()

        assert "hello, world!" in logs
        assert "bye, world!" not in logs
