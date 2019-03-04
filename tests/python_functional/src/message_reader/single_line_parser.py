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


class SingleLineParser(object):
    def __init__(self):
        self.msg_list = []
        self.__parse_rule = "\n"
        self.current_chunk = ""

    def parse_buffer(self, content_buffer):
        full_content = self.current_chunk + content_buffer
        for line in list(full_content.splitlines(True)):
            if line.endswith(self.__parse_rule):
                self.msg_list.append(line)
            else:
                self.current_chunk = line
