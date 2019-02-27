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
from src.driver_io.file.file_io import FileIO
from src.syslog_ng_config.statements.destinations.destination_driver import DestinationDriver


class FileDestination(DestinationDriver):
    def __init__(self, logger_factory, working_dir, file_name, **kwargs):
        super(FileDestination, self).__init__(logger_factory, FileIO)
        self.options = kwargs
        self.driver_name = "file"
        self.positional_option_name = "file_name"
        self.options[self.positional_option_name] = Path(working_dir, file_name)

    def get_path(self):
        return Path(self.options[self.positional_option_name])

    def read_log(self):
        return self.dd_read_logs(self.get_path(), counter=1)[0]

    def read_logs(self, counter):
        return self.dd_read_logs(self.get_path(), counter=counter)
