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
from src.common.blocking import DEFAULT_TIMEOUT
from src.common.blocking import wait_until_true_custom

READ_ALL_MESSAGES = 0


class MessageReader(object):
    def __init__(self, read, parser):
        self.__read = read
        self.__parser = parser

    def __buffer_and_parse(self, counter):
        buffered_chunk = self.__read()
        if counter == READ_ALL_MESSAGES:
            if buffered_chunk:
                self.__parser.parse_buffer(buffered_chunk)
                return False
            else:
                return True  # eof reached
        else:
            self.__parser.parse_buffer(buffered_chunk)
            return len(self.__parser.msg_list) >= counter

    def __map_counter(self, counter):
        if counter == READ_ALL_MESSAGES:
            return len(self.__parser.msg_list)
        return counter

    def pop_messages(self, counter, timeout=DEFAULT_TIMEOUT):
        assert (
            wait_until_true_custom(self.__buffer_and_parse, args=(counter,), timeout=timeout)
        ), "\nExpected message counter: [{}]\nArrived message counter: [{}]\nArrived messages: [{}]".format(
            counter, len(self.__parser.msg_list), self.__parser.msg_list
        )
        counter = self.__map_counter(counter)
        required_number_of_messages = self.__parser.msg_list[0:counter]
        self.__parser.msg_list = self.__parser.msg_list[
            counter:
        ]  # remove messages from the beginning of the msg_list, this is why we call it pop
        return required_number_of_messages

    def peek_messages(self, counter):
        assert (
            wait_until_true_custom(self.__buffer_and_parse, args=(counter,))
        ), "\nExpected message counter: [{}]\nArrived message counter: [{}]\nArrived messages: [{}]".format(
            counter, len(self.__parser.msg_list), self.__parser.msg_list
        )
        counter = self.__map_counter(counter)
        required_number_of_messages = self.__parser.msg_list[0:counter]
        return required_number_of_messages
