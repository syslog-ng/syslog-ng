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
from src.common import blocking
from src.common.blocking import wait_until_false_custom
from src.common.blocking import wait_until_true_custom

def inner_function_return_true():
    return True


def inner_function_return_false():
    return False


def inner_function_add_numbers():
    return 2 + 3


def test_wait_until_true_inner_function_returns_true():
    assert wait_until_true_custom(inner_function_return_true, timeout=0.1)


def test_wait_until_true_custom_inner_function_returns_false():
    assert wait_until_true_custom(inner_function_return_false, timeout=0.1) is False


def test_wait_until_false_inner_function_returns_true():
    assert wait_until_false_custom(inner_function_return_true, timeout=0.1) is False


def test_wait_until_false_inner_function_returns_false():
    assert wait_until_false_custom(inner_function_return_false, timeout=0.1)


def test_wait_until_true_custom_returns_with_result():
    assert wait_until_true_custom(inner_function_add_numbers, timeout=0.1) == 5
