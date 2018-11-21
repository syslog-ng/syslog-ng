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
from src.common.random_id import RandomId
from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver


class FileDestination(DestinationDriver):
    def __init__(self, logger_factory, instance_paths, **kwargs):
        super(FileDestination, self).__init__(logger_factory, FileIO)
        self.options = kwargs

        self.driver_id = "driverid_%s" % RandomId(use_static_seed=False).get_unique_id()
        self.__driver_content = {
            "driver_name": "file",
            "positional_option": "file_name",
            "driver_options": self.options,
        }
        self.__driver_node = {self.driver_id: self.__driver_content}
        self.__construct_file_path(instance_paths)

    def get_positional_option_name(self):
        return self.__driver_node[self.driver_id]["positional_option"]

    def get_positional_option_value(self):
        positional_option_name = self.get_positional_option_name()
        return self.__driver_node[self.driver_id]["driver_options"][positional_option_name]

    def get_driver_node(self):
        return self.__driver_node

    def read_log(self):
        return self.dd_read_logs(self.get_positional_option_value(), counter=1)[0]

    def read_logs(self, counter):
        return self.dd_read_logs(self.get_positional_option_value(), counter=counter)

    def get_path(self):
        return self.options[self.get_positional_option_name()]

    def __construct_file_path(self, instance_paths):
        if self.get_positional_option_name() in self.options.keys():
            given_positional_option_value = self.options[self.get_positional_option_name()]
            self.options[self.get_positional_option_name()] = Path(
                instance_paths.get_working_dir(), given_positional_option_value
            )
