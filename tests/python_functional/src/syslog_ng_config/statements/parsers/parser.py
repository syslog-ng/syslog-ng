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


class Parser(object):
    def __init__(self, driver_name, **options):
        self.driver_name = driver_name
        self.options = options
        self.positional_parameters = []
        self.group_type = "parser"


class AppParser(Parser):
    def __init__(self, **options):
        super(AppParser, self).__init__("app-parser", **options)


class SyslogParser(Parser):
    def __init__(self, **options):
        super(SyslogParser, self).__init__("syslog-parser", **options)
