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


class LogMessage(object):
    def __init__(self):
        self.priority_value = ""
        self.timestamp_value = ""
        self.hostname_value = ""
        self.program_value = ""
        self.pid_value = ""
        self.message_value = ""

    def priority(self, pri):
        self.priority_value = pri
        return self

    def remove_priority(self):
        self.priority_value = ""
        return self

    def timestamp(self, timestamp):
        self.timestamp_value = timestamp
        return self

    def hostname(self, hostname):
        self.hostname_value = hostname
        return self

    def program(self, program):
        self.program_value = program
        return self

    def pid(self, pid):
        self.pid_value = pid
        return self

    def message(self, message):
        self.message_value = message
        return self
