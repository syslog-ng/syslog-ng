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
from src.message_reader.single_line_parser import SingleLineParser
from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver
from src.syslog_ng_config.statements.option_handlers.path_based import PathBasedOptionHandler


class FileDestination(DestinationDriver):
    def __init__(self, file_name, **options):
        self.driver_name = "file"
        self.options = options
        self.__path_based_option_handler = PathBasedOptionHandler(file_name=file_name, direction="output", options=self.options)

        super(FileDestination, self).__init__(option_handler=self.__path_based_option_handler, driver_io_cls=FileIO, line_parser_cls=SingleLineParser)

    def get_path(self):
        return self.__path_based_option_handler.get_path()

    def set_path(self, new_file_name):
        self.__path_based_option_handler.set_path(new_file_name, "output")
        self.destination_reader.construct_driver_io(self.__path_based_option_handler.get_path())
