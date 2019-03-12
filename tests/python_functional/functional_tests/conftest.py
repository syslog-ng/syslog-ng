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

import logging
import pytest

from pathlib2 import Path

from src.common.operations import copy_file
from src.common.operations import calculate_testcase_name

logger = logging.getLogger(__name__)


def pytest_runtest_setup(item):
    def prepare_testcase_working_dir(pytest_request):
        testcase_parameters = pytest_request.getfixturevalue("testcase_parameters")
        working_directory = testcase_parameters.get_working_dir()
        if not working_directory.exists():
            working_directory.mkdir(parents=True)
        testcase_file_path = testcase_parameters.get_testcase_file()
        copy_file(testcase_file_path, working_directory)

    def construct_report_file_path(item):
        relative_report_dir = item._request.config.getoption("--reports")
        testcase_name = calculate_testcase_name(item._request)
        file_name = "testcase_{}.log".format(testcase_name)
        return str(Path(relative_report_dir, testcase_name, file_name).absolute())

    config = item.config
    logging_plugin = config.pluginmanager.get_plugin("logging-plugin")
    report_file_path = construct_report_file_path(item)
    logging_plugin.set_log_path(report_file_path)
    item.user_properties.append(("report_file_path", report_file_path))
    prepare_testcase_working_dir(item._request)
    item._request.addfinalizer(lambda: logger.info("Report file path\n{}\n".format(report_file_path)))
