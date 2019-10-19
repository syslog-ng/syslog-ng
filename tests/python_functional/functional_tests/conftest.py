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

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.operations import copy_file
from src.common.pytest_operations import calculate_testcase_name

logger = logging.getLogger(__name__)


def calculate_report_file_path(working_dir):
    return Path(working_dir, "testcase.reportlog")


def calculate_working_dir(pytest_config_object, testcase_name):
    report_dir = Path(pytest_config_object.getoption("--reports")).absolute()
    return Path(report_dir, calculate_testcase_name(testcase_name))


def pytest_runtest_setup(item):
    logging_plugin = item.config.pluginmanager.get_plugin("logging-plugin")
    tc_parameters.WORKING_DIR = working_dir = calculate_working_dir(item.config, item.name)
    logging_plugin.set_log_path(calculate_report_file_path(working_dir))
    item.user_properties.append(("working_dir", working_dir))
    item.user_properties.append(("relative_working_dir", working_dir.relative_to(Path.cwd())))


@pytest.fixture(autouse=True)
def setup(request):
    testcase_parameters = request.getfixturevalue("testcase_parameters")

    copy_file(testcase_parameters.get_testcase_file(), testcase_parameters.get_working_dir())
    request.addfinalizer(lambda: logger.info("Report file path\n{}\n".format(calculate_report_file_path(testcase_parameters.get_working_dir()))))
