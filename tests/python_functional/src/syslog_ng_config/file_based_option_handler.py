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
from src.syslog_ng_config.option_handler import OptionHandler


class FileBasedOptionHandler(OptionHandler):
    def __init__(self, options):
        super(FileBasedOptionHandler, self).__init__(options)
        self.file_path = None

    def init_file_path(self, file_name):
        if file_name or file_name == "":
            # note if file_name defined or file_name is ""
            self.file_path = Path(tc_parameters.WORKING_DIR, file_name)
        elif not file_name:
            # note if not file_name defined we will provide own name
            self.file_path = Path(tc_parameters.WORKING_DIR, "default_file_path")

    def get_file_path(self):
        return self.file_path

    def set_file_path(self, new_file_name):
        self.file_path = Path(tc_parameters.WORKING_DIR, new_file_name)
