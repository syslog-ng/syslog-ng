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
from pathlib import Path

from src.common.pytest_operations import calculate_testcase_name


INSTANCE_PATH = None


class TestcaseParameters(object):
    def __init__(self, pytest_request):
        testcase_name = calculate_testcase_name(pytest_request.node.name)
        absolute_framework_dir = Path(__file__).parents[2]
        self.testcase_parameters = {
            "dirs": {
                "install_dir": Path(pytest_request.config.getoption("--installdir")),
                "shared_dir": Path(absolute_framework_dir, "shared_files"),
            },
            "file_paths": {
                "testcase_file": Path(pytest_request.fspath),
            },
            "testcase_name": testcase_name,
            "external_tool": pytest_request.config.getoption("--run-under"),
        }

    def get_install_dir(self):
        return self.testcase_parameters["dirs"]["install_dir"]

    def get_shared_dir(self):
        return self.testcase_parameters["dirs"]["shared_dir"]

    def get_testcase_file(self):
        return self.testcase_parameters["file_paths"]["testcase_file"]

    def get_testcase_name(self):
        return self.testcase_parameters["testcase_name"]

    def get_external_tool(self):
        return self.testcase_parameters["external_tool"]
