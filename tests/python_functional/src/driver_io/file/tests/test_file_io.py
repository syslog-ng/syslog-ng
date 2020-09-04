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
from src.driver_io.file.file_reader import FileReader
from src.driver_io.file.file_writer import FileWriter


def test_file_io_write_read(temp_file, test_message):
    file_writer = FileWriter(temp_file)
    file_writer.write(test_message)
    file_reader = FileReader(temp_file)
    output = file_reader.read()
    assert test_message == output


def test_file_io_multiple_write_read(temp_file, test_message):
    file_writer = FileWriter(temp_file)
    file_writer.write(test_message)
    file_writer.write(test_message)
    file_reader = FileReader(temp_file)
    output = file_reader.read()
    assert test_message + test_message == output


def test_file_io_rewrite_read(temp_file):
    file_writer = FileWriter(temp_file)
    content1 = "message 1\n"
    file_writer.write(content1)
    file_reader = FileReader(temp_file)
    assert file_reader.read() == content1
    content2 = "message 2\n"
    file_writer.rewrite(content2)
    assert file_reader.read(position=0) == content2
