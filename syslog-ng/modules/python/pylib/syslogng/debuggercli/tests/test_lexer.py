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

import unittest


class TestLexer(unittest.TestCase):

    def setUp(self):
        self._lexer = self._construct_lexer()
        self._current_token = None

    def _construct_lexer(self):
        raise NotImplementedError

    def _next_token(self):
        self._current_token = self._lexer.token()
        return self._current_token

    def _assert_current_token_type_equals(self, token_type):
        self.assertEqual(self._current_token.type, token_type)

    def _assert_current_token_value_equals(self, value):
        self.assertEqual(self._current_token.value, value)

    def _assert_current_token_pos_equals(self, pos):
        self.assertEqual(self._current_token.lexpos, pos)

    def _assert_current_token_is_partial(self):
        self.assertTrue(self._current_token.partial)

    def _assert_current_token_is_not_partial(self):
        self.assertFalse(self._current_token.partial)

    def _assert_current_token_equals(self, token_type=None, value=None, pos=None, partial=None):
        if token_type is not None:
            self._assert_current_token_type_equals(token_type)
        if value is not None:
            self._assert_current_token_value_equals(value)
        if pos is not None:
            self._assert_current_token_pos_equals(pos)
        if partial is not None:
            if partial:
                self._assert_current_token_is_partial()
            else:
                self._assert_current_token_is_not_partial()

    def _assert_next_token_equals(self, *args, **kwargs):
        self._next_token()
        self._assert_current_token_equals(*args, **kwargs)

    def _assert_next_token_type_equals(self, token_type):
        self._next_token()
        self._assert_current_token_type_equals(token_type)

    def _assert_next_token_is_partial(self):
        self._next_token()
        self._assert_current_token_is_partial()

    def _assert_next_token_is_none(self):
        self.assertIsNone(self._next_token())

    def _assert_next_token_value_equals(self, value):
        self._next_token()
        self._assert_current_token_value_equals(value)
