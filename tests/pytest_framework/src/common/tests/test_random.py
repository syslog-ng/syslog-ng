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

from src.common.random_id import RandomId


def test_get_unique_id_static_seed():
    random = RandomId(use_static_seed=True)
    first_10_random_numbers = []
    for _i in range(10):
        first_10_random_numbers.append(random.get_unique_id())

    random = RandomId(use_static_seed=True)
    second_10_random_numbers = []
    for _i in range(10):
        second_10_random_numbers.append(random.get_unique_id())

    assert first_10_random_numbers == second_10_random_numbers


def test_get_unique_id_static_seed_false():
    random = RandomId(use_static_seed=False)
    first_10_random_numbers = []
    for _i in range(10):
        first_10_random_numbers.append(random.get_unique_id())

    random = RandomId(use_static_seed=False)
    second_10_random_numbers = []
    for _i in range(10):
        second_10_random_numbers.append(random.get_unique_id())

    assert first_10_random_numbers != second_10_random_numbers
