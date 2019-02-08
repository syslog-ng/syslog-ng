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

import pytest, subprocess
from pathlib2 import Path
from datetime import datetime
from src.setup.testcase import SetupTestCase
from src.setup.unit_testcase import SetupUnitTestcase


def pytest_addoption(parser):
    parser.addoption("--runslow", action="store_true", default=False, help="run slow tests")
    parser.addoption("--run-with-valgrind", action="store_true", default=False, help="Run tests behind valgrind")
    parser.addoption(
        "--loglevel",
        action="store",
        default="info",
        help="Set loglevel for test runs. Available loglevels: ['info', 'error', 'debug']. Default loglevel: info",
    )

    parser.addoption(
        "--installdir",
        action="store",
        help="Set installdir for installed syslog-ng. Used when installmode is: custom. Example path: '/home/user/syslog-ng/installdir/'",
    )
    parser.addoption(
        "--reports",
        action="store",
        default=get_relative_report_dir(),
        help="Path for report files folder. Default form: 'reports/<current_date>'",
    )


@pytest.fixture
def reports(request):
    return request.config.getoption("--reports")


@pytest.fixture(scope="session")
def installdir(request):
    return request.config.getoption("--installdir")


@pytest.fixture
def loglevel(request):
    return request.config.getoption("--loglevel")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--runslow"):
        return
    skip_slow = pytest.mark.skip(reason="need --runslow option to run")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip_slow)


@pytest.fixture
def runwithvalgrind(request):
    return request.config.getoption("--run-with-valgrind")


def get_relative_report_dir():
    return str(Path("reports/", get_current_date()))


def get_current_date():
    return datetime.now().strftime("%Y-%m-%d-%H-%M-%S-%f")


@pytest.fixture
def tc(request):
    return SetupTestCase(request)


@pytest.fixture
def tc_unittest(request):
    return SetupUnitTestcase(request, get_current_date)

@pytest.fixture(scope="session")
def version(request, installdir):
    binary_path = str(Path(installdir, "sbin", "syslog-ng"))
    version_output = subprocess.check_output([binary_path, "--version"]).decode()
    return version_output.splitlines()[1].split()[2]
