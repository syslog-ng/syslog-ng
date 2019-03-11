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


class GlobalDriver(object):
    def __init__(self, driver_name, driver_resources, options, positional_option, connection_options):
        self.driver_name = driver_name
        if driver_resources:
            self.driver_io = driver_resources['driver_io']
            self.driver_parser = driver_resources['parser']

        self.positional_option = positional_option
        self.positional_value = None

        self.connection_options = connection_options
        self.connection_values = None

        if not options:
            options = {}
        self.options = options
        if self.options:
            self.calculate_positional_value()
            self.calculate_connection_value()

    def calculate_positional_value(self):
        if self.positional_option:
            self.positional_value = self.options[self.positional_option]

    def calculate_connection_value(self):
        if self.connection_options and isinstance(self.connection_options, str):
            self.connection_values = self.options[self.connection_options]
        if self.connection_options and isinstance(self.connection_options, list):
            self.connection_values = {}
            for option_name in self.connection_options:
                self.connection_values.update({option_name: self.options[option_name]})

    def get_connection(self):
        return self.connection_values
