#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
import pytest
from src.message_reader.message_reader import MessageReader
from src.message_reader.single_line_parser import SingleLineParser
from src.common import blocking

blocking.MONITORING_TIME = 0.1


@pytest.mark.parametrize(
    "input_message_counter, requested_message_counter, expected_result",
    [
        (1, 1, True),
        (1, 2, False),
        (2, 1, True),
        (5, 0, False),  ## because we are not in wait_until_true() loop, self.read_eof will not turn to True
    ],
)
def test_buffer_and_parse(tc_unittest, input_message_counter, requested_message_counter, expected_result):
    input_content = tc_unittest.get_utf8_test_messages(input_message_counter)
    __writeable_file, readable_file = tc_unittest.prepare_input_file(input_content)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())
    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    assert message_reader._MessageReader__buffer_and_parse(requested_message_counter) is expected_result
    assert single_line_parser.msg_list == input_content.splitlines(True)


def test_multiple_buffer_and_parse(tc_unittest):
    input_content = tc_unittest.get_utf8_test_messages(2)
    writeable_file, readable_file = tc_unittest.prepare_input_file(input_content)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())
    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    assert message_reader._MessageReader__buffer_and_parse(0) is False
    writeable_file.write(input_content)
    writeable_file.flush()
    assert message_reader._MessageReader__buffer_and_parse(0) is False
    assert single_line_parser.msg_list == input_content.splitlines(True) * 2


@pytest.mark.parametrize(
    "input_message, requested_message_counter, popped_message, remaining_message",
    [
        (  # pop as many as came in buffer
            "test message 1\ntest message 2\n",
            2,
            ["test message 1\n", "test message 2\n"],
            [],
        ),
        ("test message 1\ntest message 2", 2, ["test message 1\n"], []),  # one of the messages not parsable
        (  # pop less message than came in buffer
            "test message 1\ntest message 2\n",
            1,
            ["test message 1\n"],
            ["test message 2\n"],
        ),
        (  # pop more message than came in buffer
            "test message 1\ntest message 2\n",
            10,
            ["test message 1\n", "test message 2\n"],
            [],
        ),
        (  # pop all messages from buffer, 0 = READ_ALL_MESSAGES
            "test message 1\ntest message 2\n",
            0,
            ["test message 1\n", "test message 2\n"],
            [],
        ),
        (  # remain 1 message in buffer and msg list
            "test message 1\ntest message 2\ntest message 3",
            1,
            ["test message 1\n"],
            ["test message 2\n"],
        ),
    ],
)
def test_pop_messages(tc_unittest, input_message, requested_message_counter, popped_message, remaining_message):
    __writeable_file, readable_file = tc_unittest.prepare_input_file(input_message)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())

    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    if requested_message_counter > input_message.count("\n"):
        with pytest.raises(AssertionError):
            message_reader.pop_messages(requested_message_counter)
    else:
        assert message_reader.pop_messages(requested_message_counter) == popped_message
        assert message_reader._MessageReader__parser.msg_list == remaining_message


def test_popping_in_sequence(tc_unittest):
    input_content = tc_unittest.get_utf8_test_messages(counter=10)
    __writeable_file, readable_file = tc_unittest.prepare_input_file(input_content)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())

    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    input_content_list = input_content.splitlines(True)
    assert message_reader.pop_messages(counter=2) == input_content_list[0:2]
    assert message_reader.pop_messages(counter=2) == input_content_list[2:4]
    assert message_reader.pop_messages(counter=2) == input_content_list[4:6]
    assert message_reader.pop_messages(counter=2) == input_content_list[6:8]
    assert message_reader.pop_messages(counter=2) == input_content_list[8:10]


def test_writing_popping_in_sequence(tc_unittest):
    test_message = "test message 1\n"
    writeable_file, readable_file = tc_unittest.prepare_input_file(test_message)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())

    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    assert message_reader.pop_messages(counter=1) == test_message.splitlines(True)

    test_message = "test message 2\ntest message 3\n"
    writeable_file.write(test_message)
    writeable_file.flush()
    assert message_reader.pop_messages(counter=2) == test_message.splitlines(True)

    test_message = "test message 4\ntest message 5\n"
    writeable_file.write(test_message)
    writeable_file.flush()
    assert message_reader.pop_messages(counter=1) == ["test message 4\n"]
    assert message_reader.pop_messages(counter=1) == ["test message 5\n"]

    test_message = "test message 6\ntest message 7\ntest message 8\ntest message 9\n"
    writeable_file.write(test_message)
    writeable_file.flush()
    assert message_reader.pop_messages(counter=0) == test_message.splitlines(True)

    test_message = "test message 10\n"
    writeable_file.write(test_message)
    writeable_file.flush()
    assert message_reader.pop_messages(counter=1) == test_message.splitlines(True)


def test_peek_messages(tc_unittest):
    test_message = "test message 2\ntest message 3\n"
    __writeable_file, readable_file = tc_unittest.prepare_input_file(test_message)
    single_line_parser = SingleLineParser(tc_unittest.get_fake_logger_factory())
    message_reader = MessageReader(tc_unittest.get_fake_logger_factory(), readable_file.read, single_line_parser)

    assert message_reader.peek_messages(counter=2) == test_message.splitlines(True)
    assert message_reader._MessageReader__parser.msg_list == test_message.splitlines(True)
