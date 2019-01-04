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


class StatementGroup(object):
    def __init__(self, group_type):
        self.__group_type = group_type
        self.__group_id = "%s_%s" % (group_type, RandomId(use_static_seed=False).get_unique_id())
        self.__statements = []

    @property
    def group_type(self):
        return self.__group_type

    @property
    def group_id(self):
        return self.__group_id

    @property
    def statements(self):
        return self.__statements

    def remove_statement(self, statement):
        self.statements.remove(statement)

    def update_group_with_statement(self, statement):
        self.statements.append(statement)

    def update_group_with_statements(self, statements):
        for statement in statements:
            self.update_group_with_statement(statement)
