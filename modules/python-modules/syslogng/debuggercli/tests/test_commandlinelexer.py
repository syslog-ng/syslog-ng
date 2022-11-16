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
from .test_lexer import TestLexer
from .. import commandlinelexer


class TestCommandLineLexer(TestLexer):

    def _construct_lexer(self):
        return commandlinelexer.CommandLineLexer()

    def test_lexer_returns_none_for_empty_string(self):
        self._lexer.input("")
        self._assert_next_token_is_none()

    def test_single_quote_character_is_returned_as_a_partial_token(self):
        for quote in ('"', "'"):
            self._lexer.input(quote)
            self._assert_next_token_is_partial()

    def test_pair_of_quotes_is_returned_as_an_empty_string(self):
        self._lexer.input("''")
        self._next_token()
        self._assert_current_token_is_not_partial()
        self._assert_current_token_value_equals('')

    def test_quoted_character_is_returned_as_the_character(self):
        self._lexer.input("'a'")
        self._next_token()
        self._assert_current_token_is_not_partial()
        self._assert_current_token_value_equals('a')

    def test_quoted_string_is_returned_as_the_string(self):
        self._lexer.input('"foo bar"')
        self._assert_next_token_value_equals('foo bar')

    def test_unquoted_string_is_returned_as_the_string(self):
        self._lexer.input('foo')
        self._assert_next_token_value_equals('foo')

    def test_unquoted_prefix_and_then_a_quoted_string_is_concatenated(self):
        self._lexer.input('foo"bar"')
        self._assert_next_token_value_equals('foobar')

    def test_double_quotes_allow_backslash_as_a_single_char_escape(self):
        self._lexer.input(r'"foo\""')
        self._assert_next_token_value_equals('foo"')

    def test_white_space_separates_tokens(self):
        self._lexer.input("""'foo'  "bar"  baz  """)
        self._assert_next_token_value_equals('foo')
        self._assert_next_token_value_equals('bar')
        self._assert_next_token_value_equals('baz')
        self._assert_next_token_is_none()

    def test_unclosed_string_is_returned_as_a_partial_token(self):
        self._lexer.input("""'foo' "bar" 'baz""")
        self._next_token()
        self._assert_current_token_value_equals('foo')
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals('bar')
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals('baz')
        self._assert_current_token_is_partial()

    def test_single_opened_paren_is_returned_as_a_partial_token(self):
        self._lexer.input("(")
        self._next_token()
        self._assert_current_token_value_equals('(')
        self._assert_current_token_is_partial()

    def test_pair_of_parens_is_returned_as_a_pair_of_parens(self):
        self._lexer.input("()")

        self._next_token()
        self._assert_current_token_value_equals('()')
        self._assert_current_token_is_not_partial()

    def test_token_enclosed_in_parens_is_returned_as_a_token_in_parens(self):
        self._lexer.input("(foo)")
        self._next_token()
        self._assert_current_token_value_equals('(foo)')
        self._assert_current_token_is_not_partial()

    def test_whitespace_in_parens_doesnt_terminate_the_token(self):
        self._lexer.input("(foo bar)")
        self._next_token()
        self._assert_current_token_value_equals('(foo bar)')
        self._assert_current_token_is_not_partial()

    def test_closing_paren_terminates_the_token_only_if_properly_paired(self):
        self._lexer.input("(foo bar (baz bax)) next-token")
        self._next_token()
        self._assert_current_token_value_equals('(foo bar (baz bax))')
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals('next-token')
        self._assert_current_token_is_not_partial()

    def test_one_too_many_closing_parens_is_interpreted_as_a_separate_token(self):
        self._lexer.input("(foo bar )) next-token")

        self._next_token()
        self._assert_current_token_value_equals('(foo bar )')
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals(')')
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals('next-token')
        self._assert_current_token_is_not_partial()

    def test_quotes_within_parens_are_left_intact_so_a_recursive_parsing_will_find_them(self):
        self._lexer.input("(foo 'bar baz') next-token")
        self._next_token()
        self._assert_current_token_value_equals("(foo 'bar baz')")
        self._assert_current_token_is_not_partial()

    def test_parens_in_quotes_are_not_counted_against_the_paren_balance(self):
        self._lexer.input("(foo 'bar )')    next-token")
        self._next_token()
        self._assert_current_token_value_equals("(foo 'bar )')")
        self._assert_current_token_is_not_partial()

        self._next_token()
        self._assert_current_token_value_equals("next-token")
        self._assert_current_token_is_not_partial()
