#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2019 Balabit
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
from src.common.random_id import get_unique_id
from src.syslog_ng_config.statements.option_handlers.option_handler import OptionHandler


class PathBasedOptionHandler(OptionHandler):
    def __init__(self, file_name, direction, options):
        self.__options = options
        self.set_path(file_name, direction)
        super(PathBasedOptionHandler, self).__init__(positional_option_func=self.get_path, io_option_func=self.get_path)

    def render_options(self):
        for option_name, option_value in self.__options.items():
            yield option_name, option_value

    def get_path(self):
        return self.__file_name

    def set_path(self, new_file_name, direction):
        if new_file_name == "":
            self.__file_name = "''"
        elif new_file_name is None:
            self.__file_name = Path(tc_parameters.WORKING_DIR, "{}_{}.log".format(direction, get_unique_id()))
        elif new_file_name == "skip":
            self.__file_name = None
        else:
            self.__file_name = Path(tc_parameters.WORKING_DIR, new_file_name)
