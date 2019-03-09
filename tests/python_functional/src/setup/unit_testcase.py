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

from mockito import when, unstub
from pathlib2 import Path
from src.common.operations import open_file
from src.common.random_id import get_unique_id
from src.setup.testcase_parameters import TestcaseParameters
from src.common.operations import calculate_testcase_name


class SetupUnitTestcase(object):
    def __init__(self, testcase_context, get_current_date):
        self.testcase_context = testcase_context
        self.__get_current_date = get_current_date
        self.__registered_dirs = []
        self.__registered_files = []
        self.__registered_fds = []

        self.testcase_context.addfinalizer(self.__teardown)

    def get_temp_dir(self):
        testcase_name = calculate_testcase_name(self.testcase_context)
        testcase_subdir = "{}_{}".format(self.__get_current_date(), testcase_name)
        temp_dir = Path("/tmp", testcase_subdir)
        temp_dir.mkdir()
        self.__registered_dirs.append(temp_dir)
        return temp_dir

    def get_temp_file(self):
        temp_dir = self.get_temp_dir()
        temp_file_path = Path(temp_dir, get_unique_id())
        self.__registered_files.append(temp_file_path)
        return temp_file_path

    def get_fake_testcase_parameters(self):
        self.testcase_context.config.option.installdir = self.get_temp_dir()
        self.testcase_context.config.option.reports = self.get_temp_dir()
        self.testcase_context.config.option.runwithvalgrind = False
        return TestcaseParameters(self.testcase_context)

    @staticmethod
    def get_utf8_test_messages(counter):
        utf8_test_message = "test message - öüóőúéáű"
        result_content = ""
        for i in range(1, counter + 1):
            result_content += "{} - {}\n".format(utf8_test_message, i)
        return result_content

    def prepare_input_file(self, input_content):
        input_file_path = self.get_temp_file()
        writeable_file = open_file(input_file_path, "a+")
        readable_file = open_file(input_file_path, "r")

        self.__registered_fds.append(writeable_file)
        self.__registered_fds.append(readable_file)
        writeable_file.write(input_content)
        writeable_file.flush()
        return writeable_file, readable_file

    def __teardown(self):
        for registered_fd in self.__registered_fds:
            registered_fd.close()

        for temp_file in self.__registered_files:
            if temp_file.exists():
                temp_file.unlink()

        for temp_dir in self.__registered_dirs:
            if temp_dir.exists():
                temp_dir.rmdir()

        unstub()
