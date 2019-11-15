#############################################################################
# Copyright (c) 2019 Balabit
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

from utils.OptionParser import (_find_options, _find_options_with_keyword,
                                _find_options_wo_keyword)


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'", {(3, 7)}),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' string LL_IDENTIFIER '(' argument ')' ')'", {(4, 7)}),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_PARENT_BLOCK '(' KW_OPTION '(' argument ')' ')' ')'", {(6, 9)}),
        ("LL_CONTEXT_DESTINATION KW_NETWORK '(' string KW_FAILOVER '(' KW_SERVERS '(' string_list ')' KW_FAILBACK "
         "'(' KW_TCP_PROBE_INTERVAL '(' positive_integer ')' ')' ')' ')'", {(6, 9), (12, 15)})
    ]
)
def test_find_options_with_keyword(path, options):
    assert _find_options_with_keyword(path.split()) == options


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' string LL_IDENTIFIER '(' argument ')' ')'", {(3, 3)}),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_PARENT_BLOCK '(' string number KW_OPTION '(' argument ')' ')' ')'",
         {(3, 3), (6, 7)}),
    ]
)
def test_find_options_wo_keyword(path, options):
    assert _find_options_wo_keyword(path.split()) == options


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_OPTION1 '(' argument argument ')' KW_PARENT_BLOCK '(' "
         "string number KW_OPTION '(' argument ')' ')' ')'", {(3, 3), (4, 8), (11, 12), (13, 16)}),
    ]
)
def test_find_options(path, options):
    assert _find_options(path.split()) == options
