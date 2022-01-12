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
from src.driver_io.file.file_io import FileIO


def test_file_io_write_read(temp_file, test_message):
    fileio = FileIO(temp_file)
    fileio.write(test_message)
    output = fileio.read_number_of_lines(1)
    assert [test_message] == output


def test_file_io_multiple_write_read(temp_file, test_message):
    fileio = FileIO(temp_file)
    fileio.write(test_message)
    fileio.write(test_message)
    output = fileio.read_number_of_lines(2)
    assert [test_message, test_message] == output
