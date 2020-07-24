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

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.message_reader.message_reader import MessageReader
from src.message_reader.single_line_parser import SingleLineParser
from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver


class ExampleDestination(DestinationDriver):
    def __init__(self, filename, **options):
        self.driver_name = "example-destination"
        self.path = Path(tc_parameters.WORKING_DIR, filename)
        super(ExampleDestination, self).__init__(None, dict({"filename": self.path.resolve()}, **options))

    def wait_file_content(self, content):
        with self.path.open() as f:
            message_reader = MessageReader(f.readline, SingleLineParser())

            while True:
                msg = message_reader.pop_messages(1)[0]
                if content in msg:
                    return True

            return False
