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
from src.syslog_ng_config.statements.config_statement import ConfigStatement
from src.syslog_ng_config.statements.destinations.destination_reader import DestinationReader


class DestinationDriver(ConfigStatement):
    def __init__(self, option_handler, driver_io_cls=None, line_parser_cls=None):
        self.group_type = "destination"
        if driver_io_cls:
            self.destination_reader = DestinationReader(driver_io_cls, line_parser_cls, option_handler.get_driver_io_parameter())
        else:
            self.destination_reader = None
        super(DestinationDriver, self).__init__(option_handler)

    def read_log(self):
        if self.destination_reader:
            return self.destination_reader.read_logs(counter=1)[0]

    def read_logs(self, counter):
        if self.destination_reader:
            return self.destination_reader.read_logs(counter=counter)
