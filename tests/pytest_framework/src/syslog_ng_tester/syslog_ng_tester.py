#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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

def with_file_source(variable_name, file_name):
    def decorator(cls):
        def add_file_source(self, variable_name, file_name):
            setattr(self, variable_name, self.config.create_file_source(file_name=file_name))
        cls.add_init_task(cls, add_file_source, (variable_name, file_name))
        return cls
    return decorator

def with_file_destination(variable_name, file_name):
    def decorator(cls):
        def add_file_destination(self, variable_name, file_name):
            setattr(self, variable_name, self.config.create_file_destination(file_name=file_name))
        cls.add_init_task(cls, add_file_destination, (variable_name, file_name))
        return cls
    return decorator

class SyslogNgTester(object):
    def __init__(self, testcase):
        self.testcase = testcase
        self.config = testcase.new_config()
        self.syslog_ng = testcase.new_syslog_ng()
        self.syslog_ng_ctl = testcase.new_syslog_ng_ctl()

        for func, args in self.init_functions:
            func(self, *args)

    def start(self):
        self.syslog_ng.start(self.config)

    def reopen(self):
        self.syslog_ng_ctl.reopen()

    @staticmethod
    def add_init_task(cls, func, args):
        assert cls != SyslogNgTester

        if not hasattr(cls, "init_functions"):
            cls.init_functions = []
        cls.init_functions.append((func, args))
