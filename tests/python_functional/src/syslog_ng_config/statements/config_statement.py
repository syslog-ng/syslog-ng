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


class ConfigStatement(object):
    def __init__(self, option_handler):
        self.__option_handler = option_handler

    def get_rendered_driver(self):
        rendered_driver = ""
        if self.__option_handler.get_positional_option():
            rendered_driver += "    {}\n".format(self.__option_handler.get_positional_option())

        for option_name, option_value in self.__option_handler.render_options():
            rendered_driver += "    {}({})\n".format(option_name, option_value)

        return rendered_driver
