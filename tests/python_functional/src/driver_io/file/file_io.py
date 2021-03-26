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
from src.common.file import File


class FileIO():
    def __init__(self, file_path):
        self.__readable_file = File(file_path)
        self.__writeable_file = File(file_path)

    def read_number_of_lines(self, counter):
        if not self.__readable_file.is_opened():
            if not self.__readable_file.wait_for_creation():
                raise Exception("{} was not created in time.".format(self.__readable_file.path))
            self.__readable_file.open("r")

        return self.__readable_file.wait_for_number_of_lines(counter)

    def read_until_lines(self, lines):
        if not self.__readable_file.is_opened():
            if not self.__readable_file.wait_for_creation():
                raise Exception("{} was not created in time.".format(self.__readable_file.path))
            self.__readable_file.open("r")

        return self.__readable_file.wait_for_lines(lines)

    def write(self, content):
        if not self.__writeable_file.is_opened():
            self.__writeable_file.open("a+")

        self.__writeable_file.write(content)
