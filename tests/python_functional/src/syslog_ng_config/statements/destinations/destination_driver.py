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
import logging

from src.message_reader.message_reader import MessageReader
from src.message_reader.single_line_parser import SingleLineParser

logger = logging.getLogger(__name__)


class DestinationDriver(object):
    group_type = "destination"

    def __init__(self, IOClass, positional_parameters=[], options={}):
        self.__IOClass = IOClass
        self.__reader = None
        self.positional_parameters = positional_parameters
        self.options = options


    def dd_read_logs(self, path, counter):
        if not self.__reader:
            io = self.__IOClass(path)
            io.wait_for_creation()
            message_reader = MessageReader(
                io.read, SingleLineParser()
            )
            self.__reader = message_reader
        messages = self.__reader.pop_messages(counter)
        read_description = "Content has been read from\nresource: {}\ncontent: {}\n".format(path, messages)
        logger.info(read_description)
        return messages
