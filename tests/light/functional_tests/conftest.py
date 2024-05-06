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
from pathlib import Path

import psutil
import pytest

from src.common.file import copy_file
from src.common.pytest_operations import calculate_testcase_name

logger = logging.getLogger(__name__)

base_number_of_open_fds = 0


def calculate_report_file_path(working_dir):
    return Path(working_dir, "testcase.reportlog")


def chdir_to_light_base_dir():
    absolute_light_base_dir = Path(__file__).parents[1]
    os.chdir(absolute_light_base_dir)


def get_report_dir(pytest_config_object):
    chdir_to_light_base_dir()
    return Path(pytest_config_object.getoption("--reports")).resolve().absolute()


def calculate_working_dir(pytest_config_object, testcase_name):
    report_dir = get_report_dir(pytest_config_object)
    return Path(report_dir, calculate_testcase_name(testcase_name))


def pytest_runtest_setup(item):
    logging_plugin = item.config.pluginmanager.get_plugin("logging-plugin")
    working_dir = calculate_working_dir(item.config, item.name)
    logging_plugin.set_log_path(calculate_report_file_path(working_dir))
    os.chdir(working_dir)


def pytest_sessionstart(session):
    global base_number_of_open_fds
    base_number_of_open_fds = len(psutil.Process().open_files())

    report_dir = get_report_dir(session.config)
    report_dir.mkdir(parents=True, exist_ok=True)
    if report_dir.parent.name == "reports":
        last_report_dir = Path(report_dir.parent, "last")
        last_report_dir.unlink(True)
        last_report_dir.symlink_to(report_dir)


def light_extra_files(target_dir):
    if "LIGHT_EXTRA_FILES" in os.environ:
        for f in os.environ["LIGHT_EXTRA_FILES"].split(":"):
            if Path(f).exists():
                copy_file(f, target_dir)


@pytest.fixture(autouse=True)
def setup(request):
    global base_number_of_open_fds
    number_of_open_fds = len(psutil.Process().open_files())
    assert base_number_of_open_fds + 1 == number_of_open_fds, "Previous testcase has unclosed opened fds"
    assert len(psutil.Process().connections(kind="inet")) == 0, "Previous testcase has unclosed opened sockets"
    testcase_parameters = request.getfixturevalue("testcase_parameters")

    copy_file(testcase_parameters.get_testcase_file(), Path.cwd())
    light_extra_files(Path.cwd())
    request.addfinalizer(lambda: logger.info("Report file path\n{}\n".format(calculate_report_file_path(Path.cwd()))))


class PortAllocator():
    CURRENT_DYNAMIC_PORT = 30000


@pytest.fixture(scope="session")
def port_allocator():
    def get_next_port():
        PortAllocator.CURRENT_DYNAMIC_PORT += 1
        return PortAllocator.CURRENT_DYNAMIC_PORT

    return get_next_port
