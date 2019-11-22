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
from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.statements.sources.source_driver import SourceDriver
from src.syslog_ng_config.statements.sources.source_writer import SourceWriter


class FileSource(SourceDriver):
    def __init__(self, file_name, **options):
        self.driver_name = "file"
        self.path = Path(tc_parameters.WORKING_DIR, file_name)
        super(FileSource, self).__init__([self.path], options)
        self.source_writer = SourceWriter(FileIO)

    def get_path(self):
        return self.path

    def set_path(self, pathname):
        self.path = Path(tc_parameters.WORKING_DIR, pathname)

    def write_log(self, formatted_log, counter=1):
        self.source_writer.write_log(self.get_path(), formatted_log, counter=counter)
