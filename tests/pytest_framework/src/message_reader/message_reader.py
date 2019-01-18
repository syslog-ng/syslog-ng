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

from src.common.blocking import wait_until_true_custom
from src.common.blocking import DEFAULT_TIMEOUT


class MessageReader(object):
    def __init__(self, logger_factory, read, parser):
        self.__logger = logger_factory.create_logger("MessageReader")
        self.__read_all_messages = 0
        self.__read = read
        self.__parser = parser
        self.__read_eof = False

    def __buffer_and_parse(self, counter):
        buffered_chunk = self.__read()
        if buffered_chunk:
            self.__parser.parse_buffer(buffered_chunk)
        else:
            self.__read_eof = True
        if counter != self.__read_all_messages:
            return len(self.__parser.msg_list) >= counter
        retval = self.__read_eof
        self.__read_eof = False
        return retval

    def __map_counter(self, counter):
        if counter == self.__read_all_messages:
            return len(self.__parser.msg_list)
        return counter

    def pop_messages(self, counter, timeout=DEFAULT_TIMEOUT):
        assert wait_until_true_custom(self.__buffer_and_parse, args=(counter,), timeout=timeout) is True
        counter = self.__map_counter(counter)
        required_number_of_messages = self.__parser.msg_list[0:counter]
        self.__parser.msg_list = self.__parser.msg_list[
            counter:
        ]  # remove messages from the beginning of the msg_list, this is why we call it pop
        return required_number_of_messages

    def peek_messages(self, counter):
        assert wait_until_true_custom(self.__buffer_and_parse, args=(counter,)) is True
        counter = self.__map_counter(counter)
        required_number_of_messages = self.__parser.msg_list[0:counter]
        return required_number_of_messages
