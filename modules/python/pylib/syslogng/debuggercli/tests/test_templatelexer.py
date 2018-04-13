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
from .. import templatelexer
from .test_lexer import TestLexer


class TestTemplateLexer(TestLexer):

    def _construct_lexer(self):
        return templatelexer.TemplateLexer()

    def test_template_literals_are_returned_as_literal_tokens(self):
        self._lexer.input("foobar barfoo")
        self._assert_next_token_equals("LITERAL", value="foobar barfoo", pos=0)

    def test_template_unbraced_macros_are_returned_as_macros(self):
        self._lexer.input("$MSG")
        self._assert_next_token_equals("MACRO", value="$MSG", pos=0)

    def test_template_braced_macros_are_returned_as_macros(self):
        self._lexer.input("${MSG}")
        self._assert_next_token_equals("MACRO", value="${MSG}", pos=0)

    def test_template_simple_template_functions_are_returned_as_template_funcs(self):
        self._lexer.input("$(echo $MSG)")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(echo $MSG)", pos=0)

    def test_template_complex_template_functions_are_returned_as_template_funcs(self):
        self._lexer.input("$(echo $(echo $MSG))")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(echo $(echo $MSG))", pos=0)

    def test_template_even_more_complex_template_functions_are_returned_as_template_funcs(self):
        self._lexer.input("$(grep ('$COUNT' == 5) $MSG)")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(grep ('$COUNT' == 5) $MSG)", pos=0)

    def test_stray_dollar_at_the_end_of_input_is_returned_as_a_literal_token_with_partial_set(self):
        self._lexer.input("$")
        self._assert_next_token_equals("LITERAL", value="$", pos=0, partial=True)

    def test_stray_dollar_brace_at_the_end_of_input_is_returned_as_a_literal_token_with_partial_set(self):
        self._lexer.input("${")
        self._assert_next_token_equals("LITERAL", value="${", pos=0, partial=True)

    def test_stray_dollar_paren_at_the_end_of_input_is_returned_as_a_literal_token_with_partial_set(self):
        self._lexer.input("$(")
        self._assert_next_token_equals("LITERAL", value="$(", pos=0, partial=True)

    def test_unfinished_template_function_at_the_end_of_input_is_returned_as_a_literal_token_with_partial_set(self):
        self._lexer.input("$(echo")
        self._assert_next_token_equals("LITERAL", value="$(echo", pos=0, partial=True)

        self._lexer.input("$(echo $(echo $MSG)")
        self._assert_next_token_equals("LITERAL", value="$(echo $(echo $MSG)", pos=0, partial=True)

    def test_string_within_parens_returned_intact(self):
        self._lexer.input("""$("foobar")""")
        self._assert_next_token_equals("TEMPLATE_FUNC", value='$("foobar")', pos=0)

        self._lexer.input("""$("foo\\"bar")""")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="""$("foo\\"bar")""", pos=0)
        self._lexer.input("""$("foo\\")bar")""")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="""$("foo\\")bar")""", pos=0)

        self._lexer.input("""$('foobar')""")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$('foobar')", pos=0)

        self._lexer.input("""$('foo"bar')""")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="""$('foo"bar')""", pos=0)

    def test_quoted_parens_dont_close_the_template_function(self):
        self._lexer.input("$(echo ')'")
        self._assert_next_token_equals("LITERAL", value="$(echo ')'", pos=0, partial=True)

        self._lexer.input("$(echo ')')")
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(echo ')')", pos=0)

    def test_quotes_outside_template_functions_are_returned_literally(self):
        self._lexer.input("'$(echo $MSG)'")
        self._assert_next_token_equals("LITERAL", value="'", pos=0)
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(echo $MSG)", pos=1)
        self._assert_next_token_equals("LITERAL", value="'", pos=13)

    def test_braced_macro_splits_literals_in_half(self):
        self._lexer.input("foobar${MSG}barfoo")
        self._assert_next_token_equals("LITERAL", value="foobar", pos=0)
        self._assert_next_token_equals("MACRO", value="${MSG}", pos=6)
        self._assert_next_token_equals("LITERAL", value="barfoo", pos=12)

    def test_unbraced_macros_split_literals_in_half(self):
        self._lexer.input("foobar $MSG barfoo")
        self._assert_next_token_equals("LITERAL", value="foobar ", pos=0)
        self._assert_next_token_equals("MACRO", value="$MSG", pos=7)
        self._assert_next_token_equals("LITERAL", value=" barfoo", pos=11)

    def test_template_function_splits_literals_in_half(self):
        self._lexer.input("foobar$(echo $MSG)barfoo")
        self._assert_next_token_equals("LITERAL", value="foobar", pos=0)
        self._assert_next_token_equals("TEMPLATE_FUNC", value="$(echo $MSG)", pos=6)
        self._assert_next_token_equals("LITERAL", value="barfoo", pos=18)
