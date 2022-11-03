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

from __future__ import absolute_import, print_function
from ..templatelang import TemplateLang
from .test_completerlang import CompleterLangTestCase


class TestTemplateLang(CompleterLangTestCase):
    def setUp(self):
        self._parser = TemplateLang()

    def test_template_is_a_series_of_literal_macro_and_template_funcs(self):
        self._assert_token_follows("", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows(" ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("foo ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("$MSG ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("${MSG} ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("$(echo $MSG) ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("foo$MSG${MSG}$(echo $MSG)bar ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
