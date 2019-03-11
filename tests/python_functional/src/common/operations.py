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
import os
import shutil

from pathlib2 import Path


def open_file(file_path, mode):
    # Python 2 compatibility note: open() can work only with string representation of path
    return open(str(file_path), mode)


def open_named_pipe(file_path):
    return os.open(str(file_path), os.O_RDONLY | os.O_NONBLOCK)


def copy_file(src_file_path, dst_dir):
    # Python 2 compatibility note: shutil.copy() can work only with string representation of path
    shutil.copy(str(src_file_path), str(dst_dir))


def cast_to_list(items):
    if isinstance(items, list):
        return items
    return [items]


def copy_shared_file(shared_file_name, syslog_ng_testcase):
    shared_dir = syslog_ng_testcase.testcase_parameters.get_shared_dir()
    working_dir = syslog_ng_testcase.testcase_parameters.get_working_dir()
    copy_file(Path(shared_dir, shared_file_name), working_dir)


def delete_session_file(shared_file_name, syslog_ng_testcase):
    working_dir = syslog_ng_testcase.testcase_parameters.get_working_dir()
    shared_file_name = Path(working_dir, shared_file_name)
    shared_file_name.unlink()


def calculate_testcase_name(pytest_request):
    # In case of parametrized tests we need to replace "[" and "]" because
    # testcase name will appear in directory name
    return pytest_request.node.name.replace("[", "_").replace("]", "_")
