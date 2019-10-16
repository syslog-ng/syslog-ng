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
import subprocess
from datetime import datetime

import pytest
from pathlib2 import Path

from src.message_builder.bsd_format import BSDFormat
from src.message_builder.log_message import LogMessage
from src.syslog_ng.syslog_ng import SyslogNg
from src.syslog_ng.syslog_ng_paths import SyslogNgPaths
from src.syslog_ng_config.syslog_ng_config import SyslogNgConfig
from src.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.testcase_parameters.testcase_parameters import TestcaseParameters

logger = logging.getLogger(__name__)


# Command line options
def pytest_addoption(parser):
    parser.addoption("--runslow", action="store_true", default=False, help="run slow tests")
    parser.addoption("--run-under", help="Run syslog-ng under selected tool, example tools: [valgrind, strace]")
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


def get_relative_report_dir():
    return str(Path("reports/", get_current_date()))


def get_current_date():
    return datetime.now().strftime("%Y-%m-%d-%H-%M-%S-%f")


# Pytest Hooks
def pytest_collection_modifyitems(config, items):
    if config.getoption("--runslow"):
        return
    skip_slow = pytest.mark.skip(reason="need --runslow option to run")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip_slow)


def pytest_runtest_logreport(report):
    if report.outcome == "failed":
        logger.error("\n{}".format(report.longrepr))


# Pytest Fixtures
@pytest.fixture
def testcase_parameters(request):
    return TestcaseParameters(request)


@pytest.fixture
def config(request):
    return SyslogNgConfig(request.getfixturevalue("version"))


@pytest.fixture
def syslog_ng(request, testcase_parameters):
    instance_paths = SyslogNgPaths(testcase_parameters).set_syslog_ng_paths("server")
    syslog_ng = SyslogNg(instance_paths, testcase_parameters)
    request.addfinalizer(lambda: syslog_ng.stop())
    return syslog_ng


@pytest.fixture
def syslog_ng_ctl(syslog_ng):
    return SyslogNgCtl(syslog_ng.instance_paths)


@pytest.fixture
def bsd_formatter():
    return BSDFormat()


@pytest.fixture
def log_message():
    return LogMessage()


@pytest.fixture(scope="session")
def version(request):
    installdir = request.config.getoption("--installdir")
    binary_path = str(Path(installdir, "sbin", "syslog-ng"))
    version_output = subprocess.check_output([binary_path, "--version"]).decode()
    return version_output.splitlines()[1].split()[2]
