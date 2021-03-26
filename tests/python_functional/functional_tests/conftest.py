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
import os

import pytest
from pathlib2 import Path

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.common.file import copy_file
from src.common.pytest_operations import calculate_testcase_name

logger = logging.getLogger(__name__)


def calculate_report_file_path(working_dir):
    return Path(working_dir, "testcase.reportlog")


def calculate_working_dir(pytest_config_object, testcase_name):
    report_dir = Path(pytest_config_object.getoption("--reports")).resolve().absolute()
    return Path(report_dir, calculate_testcase_name(testcase_name))


def working_dir_and_current_dir_has_common_base(working_dir):
    return str(working_dir).startswith(str(Path.cwd()))


def pytest_runtest_setup(item):
    logging_plugin = item.config.pluginmanager.get_plugin("logging-plugin")
    tc_parameters.WORKING_DIR = working_dir = calculate_working_dir(item.config, item.name)
    logging_plugin.set_log_path(calculate_report_file_path(working_dir))
    item.user_properties.append(("working_dir", working_dir))
    if working_dir_and_current_dir_has_common_base(working_dir):
        # relative path for working dir could be calculeted from current directory
        relative_working_dir = working_dir.relative_to(Path.cwd())
        item.user_properties.append(("relative_working_dir", relative_working_dir))
    else:
        # relative path for working dir could not be calculeted from current directory
        if len(str(working_dir)) + len("syslog_ng_server.ctl") > 108:
            # #define UNIX_PATH_MAX	108 (cat /usr/include/linux/un.h | grep "define UNIX_PATH_MAX)"
            raise ValueError("Working directory lenght is too long, some socket files could not be saved, please make it shorter")
        item.user_properties.append(("relative_working_dir", working_dir))


def light_extra_files(target_dir):
    if "LIGHT_EXTRA_FILES" in os.environ:
        for f in os.environ["LIGHT_EXTRA_FILES"].split(":"):
            if Path(f).exists():
                copy_file(f, target_dir)


@pytest.fixture(autouse=True)
def setup(request):
    testcase_parameters = request.getfixturevalue("testcase_parameters")

    copy_file(testcase_parameters.get_testcase_file(), testcase_parameters.get_working_dir())
    light_extra_files(testcase_parameters.get_working_dir())
    request.addfinalizer(lambda: logger.info("Report file path\n{}\n".format(calculate_report_file_path(testcase_parameters.get_working_dir()))))


class PortAllocator():
    CURRENT_DYNAMIC_PORT = 30000


@pytest.fixture(scope="session")
def port_allocator():
    def get_next_port():
        PortAllocator.CURRENT_DYNAMIC_PORT += 1
        return PortAllocator.CURRENT_DYNAMIC_PORT

    return get_next_port
