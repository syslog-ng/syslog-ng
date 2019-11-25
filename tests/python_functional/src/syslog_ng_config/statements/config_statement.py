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


DEFAULT_DRIVER_INDENTATION = " " * 2 * 4


class ConfigStatement(object):
    def __init__(self):
        self.rendered_driver_config = None

    def render_driver_options(self):
        self.rendered_driver_config = ""
        self.render_positional_options()
        self.render_options(self.options)
        return self.rendered_driver_config

    def render_positional_options(self):
        if self.positional_parameters and self.positional_parameters[0]:
            self.rendered_driver_config += "{}{}\n".format(DEFAULT_DRIVER_INDENTATION, self.positional_parameters[0])

    def render_options(self, options, indentation=DEFAULT_DRIVER_INDENTATION):
        for option_name, option_value in options.items():
            if isinstance(option_value, dict):
                inner_block_indentation = " " * 4
                self.rendered_driver_config += "{}{}(\n".format(indentation, option_name)
                self.render_options(option_value, indentation=indentation + inner_block_indentation)
                self.rendered_driver_config += "{})\n".format(indentation)
            else:
                self.rendered_driver_config += "{}{}({})\n".format(indentation, option_name, option_value)
