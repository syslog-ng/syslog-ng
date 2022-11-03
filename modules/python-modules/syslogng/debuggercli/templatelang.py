#############################################################################
# Copyright (c) 2015-2016 Balabit
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

from __future__ import print_function, absolute_import
from .completerlang import CompleterLang
from .templatelexer import TemplateLexer


class TemplateLang(CompleterLang):

    tokens = [
        "LITERAL", "MACRO", "TEMPLATE_FUNC"
    ]

    @staticmethod
    def p_template_string(p):
        '''template_string : template_elems'''
        pass

    @staticmethod
    def p_template_elems(p):
        '''template_elems : template_elem template_elems
                          |'''
        pass

    @staticmethod
    def p_template_elem(p):
        '''template_elem : LITERAL
                         | MACRO
                         | TEMPLATE_FUNC
        '''
        pass

    def _construct_lexer(self):
        return TemplateLexer()
