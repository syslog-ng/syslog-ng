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

from pathlib2 import Path


def get_testcase_name(testcase_context):
    if testcase_context.node.name is not None:
        return testcase_context.node.name.replace("[", "_").replace("]", "_")
    elif testcase_context.node.originalname is not None:
        return testcase_context.node.originalname.replace("[", "_").replace("]", "_")
    else:
        raise Exception("There is no valid testcasename")


class TestcaseParameters(object):
    def __init__(self, testcase_context):
        testcase_name = get_testcase_name(testcase_context)
        relative_report_dir = testcase_context.config.getoption("--reports")
        absolute_framework_dir = Path.cwd()

        self.testcase_parameters = {
            "dirs": {
                "working_dir": Path(absolute_framework_dir, relative_report_dir, testcase_name),
                "relative_working_dir": Path(relative_report_dir, testcase_name),
                "install_dir": Path(testcase_context.config.getoption("--installdir")),
            },
            "file_paths": {
                "report_file": Path(
                    absolute_framework_dir, relative_report_dir, testcase_name, "testcase_{}.log".format(testcase_name)
                ),
                "testcase_file": Path(testcase_context.node.fspath),
            },
            "testcase_name": testcase_name,
            "loglevel": testcase_context.config.getoption("--loglevel"),
            "valgrind_usage": testcase_context.config.getoption("--run-with-valgrind"),
        }

    def get_working_dir(self):
        return self.testcase_parameters["dirs"]["working_dir"]

    def get_relative_working_dir(self):
        return self.testcase_parameters["dirs"]["relative_working_dir"]

    def get_install_dir(self):
        return self.testcase_parameters["dirs"]["install_dir"]

    def get_report_file(self):
        return self.testcase_parameters["file_paths"]["report_file"]

    def get_testcase_file(self):
        return self.testcase_parameters["file_paths"]["testcase_file"]

    def get_testcase_name(self):
        return self.testcase_parameters["testcase_name"]

    def get_loglevel(self):
        return self.testcase_parameters["loglevel"]

    def get_valgrind_usage(self):
        return self.testcase_parameters["valgrind_usage"]
