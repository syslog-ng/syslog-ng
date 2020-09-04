#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2020 Balabit
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
import logging

from src.driver_io.file.file import File
from src.message_reader.message_reader import MessageReader
from src.message_reader.message_reader import READ_ALL_AVAILABLE_MESSAGES
from src.message_reader.single_line_parser import SingleLineParser

logger = logging.getLogger(__name__)


class FileReader(File):
    def __init__(self, file_path):
        super(FileReader, self).__init__(file_path)
        self.__readable_file = None

    def read(self, position=None):
        if not self.__readable_file:
            self.__readable_file = self.open_file(mode="r")

        if position is not None:
            self.__readable_file.seek(position)

        content = self.__readable_file.read()
        return content

    def readline(self):
        if not self.__readable_file:
            self.__readable_file = self.open_file(mode="r")

        return self.__readable_file.readline()

    def read_log(self):
        return self.read_logs(counter=1)[0]

    def read_logs(self, counter):
        self.wait_for_creation()
        message_reader = MessageReader(self.read, SingleLineParser())
        self.__reader = message_reader
        messages = self.__reader.pop_messages(counter)
        read_description = "Content has been read from\nresource: {}\ncontent: {}\n".format(self.file_path, messages)
        logger.info(read_description)
        return messages

    def read_all_logs(self):
        return self.read_logs(READ_ALL_AVAILABLE_MESSAGES)
