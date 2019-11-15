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


def _find_options_with_keyword(path):
    options = set()
    option_start = None
    for index, token in enumerate(path):
        if token == "'('" and index != 2:
            option_start = index - 1
        elif token == "')'" and option_start is not None:
            options.add((option_start, index))
            option_start = None
    return options


def _find_options_wo_keyword(path):
    options = set()
    left_brace = None
    for index, token in enumerate(path):
        if token == "'('":
            if left_brace is None:
                left_brace = index
                continue
            option_start = left_brace + 1
            option_end = index - 2
            if option_start <= option_end:
                options.add((option_start, option_end))
            left_brace = index
        elif token == "')'":
            left_brace = None
    return options


def _find_options(path):
    return _find_options_wo_keyword(path) | _find_options_with_keyword(path)
