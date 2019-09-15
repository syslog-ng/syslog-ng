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
from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.statements.option_handlers.path_based import PathBasedOptionHandler
from src.syslog_ng_config.statements.sources.source_driver import SourceDriver


class FileSource(SourceDriver):
    def __init__(self, file_name, **options):
        self.driver_name = "file"
        self.options = options
        self.__path_based_option_handler = PathBasedOptionHandler(file_name=file_name, direction="input", options=self.options)

        super(FileSource, self).__init__(option_handler=self.__path_based_option_handler, driver_io_cls=FileIO)

    def get_path(self):
        return self.__path_based_option_handler.get_path()

    def set_path(self, new_file_name):
        self.__path_based_option_handler.set_path(new_file_name, "input")
        self.source_writer.construct_driver_io(self.__path_based_option_handler.get_path())
