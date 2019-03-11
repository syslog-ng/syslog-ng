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
import os

from src.driver_io.file.file import File


class PipeIO(File):
    def __init__(self, connection_values):
        super(PipeIO, self).__init__(connection_values)
        self.connection_values = connection_values
        self.__readable_file = None
        self.__writeable_file = None

    def read(self):
        if not self.__readable_file:
            self.__readable_file = self.open_named_pipe()
        merged_pipe_content = ""
        while True:
            try:
                pipe_content = os.read(self.__readable_file, 1024)
                if pipe_content:
                    merged_pipe_content += pipe_content.decode('utf-8')
                else:
                    break
            except OSError:  # python2
                break
            except BlockingIOError:  # python3
                break
        return merged_pipe_content

    def write(self, content):
        if self.__writeable_file is None:
            self.__writeable_file = self.open_file(mode="w")
        self.__write(content, self.__writeable_file)

    def rewrite(self, content):
        rewriteable_file = self.open_file(mode="w")
        self.__write(content, rewriteable_file)

    @staticmethod
    def __write(content, writeable_file):
        writeable_file.write(content)
        writeable_file.flush()
