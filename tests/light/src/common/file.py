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
import shutil
from pathlib import Path

from src.common.blocking import DEFAULT_TIMEOUT
from src.common.blocking import wait_until_true
from src.common.blocking import wait_until_true_custom

logger = logging.getLogger(__name__)


def open_file(file_path, mode):
    return open(file_path, mode, errors="backslashreplace")


def copy_file(src_file_path, dst_dir):
    shutil.copy(src_file_path, dst_dir)


def copy_shared_file(testcase_parameters, shared_file_name):
    shared_dir = testcase_parameters.get_shared_dir()
    copy_file(Path(shared_dir, shared_file_name), Path.cwd())
    return Path(Path.cwd(), shared_file_name)


def delete_session_file(shared_file_name):
    shared_file_name = Path(shared_file_name)
    shared_file_name.unlink()


class File(object):
    def __init__(self, file_path):
        self.path = Path(file_path)
        self.__opened_file = None

    def wait_for_creation(self):
        file_created = wait_until_true(self.path.exists)
        if file_created:
            logger.debug("File has been created: {}".format(self.path))
        else:
            raise Exception("File was not created in time: {}".format(self.path))
        return file_created

    def open(self, mode):
        self.__opened_file = open_file(self.path, mode)
        return self.__opened_file

    def close(self):
        if self.is_opened():
            self.__opened_file.close()
            self.__opened_file = None

    def is_opened(self):
        return self.__opened_file is not None

    def readline(self):
        if not self.is_opened():
            raise Exception("File was not opened before trying to read from it.")
        return self.__opened_file.readline()

    def read_full_line(self, timeout=DEFAULT_TIMEOUT):
        def read_full_line_segmented(f, line_segments):
            line_read = f.readline()
            if not line_read:
                if len(line_segments) > 0:
                    # Has to be called again to collect the rest of the line
                    return False
                # EOF reached immediately
                return True

            line_segments.append(line_read)
            return line_read[-1] == "\n"

        if not self.is_opened():
            raise Exception("File was not opened before trying to read from it.")

        line_segments = []
        wait_until_true_custom(read_full_line_segmented, (self.__opened_file, line_segments), timeout=timeout, poll_freq=0)
        return "".join(line_segments)

    def read(self):
        content = ""
        while True:
            buffer = self.__opened_file.read()
            if buffer == "":
                break
            content += buffer
        return content

    def write(self, content):
        self.__opened_file.write(content)
        self.__opened_file.flush()

    def write_content_and_close(self, content):
        if not self.is_opened():
            self.open(mode="w+")
        self.write(content)
        self.close()

    def wait_for_lines(self, lines, timeout=DEFAULT_TIMEOUT):
        def find_lines_in_file(lines_to_find, lines_found, f):
            line_read = self.read_full_line(timeout=timeout)
            if not line_read and lines_to_find:
                return False
            for line_to_find in lines_to_find:
                if line_to_find in line_read:
                    lines_found.append(line_read)
                    lines_to_find.remove(line_to_find)
                    break
            everything_is_found = lines_to_find == []
            return everything_is_found

        lines_found = []
        if not wait_until_true_custom(find_lines_in_file, (lines, lines_found, self), timeout=timeout, poll_freq=0):
            raise Exception("Could not find all lines in {}. Remaining lines to find: {} Lines found: {}".format(self.path, lines, lines_found))

        return lines_found

    def wait_for_number_of_lines(self, number_of_lines, timeout=DEFAULT_TIMEOUT):
        lines = ["\n"] * number_of_lines
        try:
            return self.wait_for_lines(lines, timeout)
        except Exception:
            raise Exception("Could not find {} number of lines in {}. Remaining number of lines: {}.".format(number_of_lines, self.path, len(lines)))
