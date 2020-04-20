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
from ..lexertoken import LexerToken
from ..tablexer import TabLexer
from ..lexer import Lexer
from .test_lexer import TestLexer


class GenNumbersLexer(Lexer):
    def __init__(self, num_tokens, mark_last_partial=False):
        self._token_count = 0
        self._num_tokens = num_tokens
        self._mark_last_partial = mark_last_partial
        self._current_position = 0

    def token(self):
        if self._token_count < self._num_tokens:
            lexpos = self._current_position
            self._token_count += 1
            self._current_position = self._token_count * 2

            return LexerToken(type="NUMBER",
                              value=str(self._token_count),
                              partial=self._mark_last_partial and self._token_count == self._num_tokens,
                              lexpos=lexpos)
        return None

    def input(self, text):
        self._current_position = 0
        self._token_count = 0

    def get_position(self):
        return self._current_position


class TestTabLexer(TestLexer):
    def setUp(self):
        self._lexer = self._construct_lexer()

    # pylint: disable=arguments-differ
    def _construct_lexer(self, num_tokens=5, mark_last_partial=False):
        lexer = TabLexer(GenNumbersLexer(num_tokens, mark_last_partial=mark_last_partial))
        self._consume_all_tokens(lexer)
        lexer.input('')
        return lexer

    @staticmethod
    def _consume_all_tokens(lexer):
        while lexer.token() is not None:
            pass

    def test_tab_lexer_token_returns_none_if_the_underlying_lexer_is_empty(self):
        self._lexer = self._construct_lexer(0)
        self._assert_next_token_is_tab(tab_position=0)
        self.assertEqual(self._lexer.token(), None)

    def test_tab_lexer_token_returns_token_if_the_underlying_lexer_has_elements(self):
        self._lexer = self._construct_lexer(2)
        self.assertEqual(self._lexer.token().value, '1')
        self.assertEqual(self._lexer.token().value, '2')
        self._assert_next_token_is_tab(tab_position=4)
        self.assertIsNone(self._lexer.token())

    def test_partial_is_replaced_by_tab(self):
        self._lexer = self._construct_lexer(2, mark_last_partial=True)
        self.assertEqual(self._lexer.token().value, '1')
        self._assert_next_token_is_tab(tab_position=2)
        self.assertIsNone(self._lexer.token())
        self.assertEqual(self._lexer.get_replaced_token().value, '2')

    def test_tab_is_appended_if_no_partial_token(self):
        self._lexer = self._construct_lexer(2, mark_last_partial=False)
        self._lexer.set_drop_last_token(False)
        self.assertEqual(self._lexer.token().value, '1')
        self.assertEqual(self._lexer.token().value, '2')
        self._assert_next_token_is_tab(tab_position=4)
        self.assertIsNone(self._lexer.token())
        self.assertIsNone(self._lexer.get_replaced_token())

    def test_last_token_is_replaced_by_a_tab_if_theres_no_partial_and_after_last_insertion_is_not_requested(self):
        self._lexer = self._construct_lexer(2, mark_last_partial=False)
        self._lexer.set_drop_last_token(True)
        self.assertEqual(self._lexer.token().value, '1')
        self._assert_next_token_is_tab(tab_position=2)
        self.assertIsNone(self._lexer.token())
        self.assertEqual(self._lexer.get_replaced_token().value, '2')

    def test_tab_lexer_returns_tab_token_at_the_end_of_the_sequence(self):
        self._lexer = self._construct_lexer(2)
        self._assert_n_numbered_tokens_available(2)
        self._assert_next_token_is_tab(tab_position=4)
        self.assertIsNone(self._lexer.token())

    def test_tab_lexer_can_replace_last_token_to_a_tab(self):
        self._lexer = self._construct_lexer(0)
        self._lexer.set_drop_last_token(True)
        self._assert_next_token_is_tab(tab_position=0)
        self.assertIsNone(self._lexer.token())

        self._lexer = self._construct_lexer(1)
        self._lexer.set_drop_last_token(True)
        self._assert_next_token_is_tab(tab_position=0)
        self.assertIsNone(self._lexer.token())

        self._lexer = self._construct_lexer(2)
        self._lexer.set_drop_last_token(True)
        self._assert_n_numbered_tokens_available(1)
        self._assert_next_token_is_tab(tab_position=2)
        self.assertIsNone(self._lexer.token())

        self._lexer = self._construct_lexer(3)
        self._lexer.set_drop_last_token(True)
        self._assert_n_numbered_tokens_available(2)
        self._assert_next_token_is_tab(tab_position=4)
        self.assertIsNone(self._lexer.token())

    def _assert_n_numbered_tokens_available(self, n):
        for i in range(1, n + 1):
            self.assertEqual(self._lexer.token().value, str(i))

    def _assert_current_token_position_equals(self, lexpos):
        self.assertEqual(self._current_token.lexpos, lexpos)

    def _assert_next_token_is_tab(self, tab_position=None):
        self._next_token()
        self._assert_current_token_type_equals("TAB")
        if tab_position is not None:
            self._assert_current_token_position_equals(tab_position)
