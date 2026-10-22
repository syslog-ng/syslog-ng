#############################################################################
# Copyright (c) 2025 One Identity
# Copyright (c) 2025 Narkhov Evgeny <evgenynarkhov2@gmail.com>
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
import pytest

import src.testcase_parameters.testcase_parameters as tc_parameters
from src.bindings.python.python_module import DummyPythonModule


@pytest.fixture
def python_module(testcase_parameters):
    module = DummyPythonModule(tc_parameters.INSTANCE_PATH.get_python_source_dir())
    yield module
    module.teardown()
