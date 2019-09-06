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
from src.common.random_id import get_unique_id
from src.message_builder.log_message import LogMessage

logger = logging.getLogger(__name__)


def pytest_runtest_setup(item):
    def prepare_testcase_working_dir(pytest_request):
        testcase_parameters = pytest_request.getfixturevalue("testcase_parameters")
        tc_parameters.WORKING_DIR = working_directory = testcase_parameters.get_working_dir()
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


@pytest.fixture
def generate_msgs_for_dummy_src_and_dst(bsd_formatter):
    input_messages = []
    expected_messages = []

    for counter in range(0, 3):
        uniq_message_content = get_unique_id()
        log_message = LogMessage().message(uniq_message_content)
        input_messages.append(bsd_formatter.format_message(log_message))
        expected_messages.append(bsd_formatter.format_message(log_message.remove_priority()))
    return input_messages, expected_messages


@pytest.fixture
def generate_msgs_for_dummy_src_and_file_dst(generate_msgs_for_dummy_src_and_dst):
    return generate_msgs_for_dummy_src_and_dst


@pytest.fixture
def generate_msgs_for_file_src_and_dummy_dst(generate_msgs_for_dummy_src_and_dst):
    return generate_msgs_for_dummy_src_and_dst
