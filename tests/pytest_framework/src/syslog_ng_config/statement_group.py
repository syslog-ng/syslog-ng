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

import functools
from src.common.random_id import get_unique_id
from src.common.operations import cast_to_list

class StatementGroup(object):
    def __init__(self, statements):
        self.__group_type = self.__calculate_group_type(cast_to_list(statements))
        self.__group_id = "%s_%s" % (self.__group_type, get_unique_id())
        self.__statements = []
        if statements:
            self.update_group_with_statements(cast_to_list(statements))

    @staticmethod
    def __calculate_group_type(statements):
        def check_consistency(stmt1, stmt2):
            type1 = stmt1.group_type
            type2 = stmt2.group_type
            if type1 != type2:
                raise TypeError("Conflict in statement types: {} and {}".format(type1, type2))
            return stmt2

        return functools.reduce(check_consistency, statements).group_type

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
