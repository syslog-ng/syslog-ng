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
from .test_completer import CompleterTestCase
from ..langcompleter import LangBasedCompleter
from ..completerlang import CompleterLang
from ..choicecompleter import ChoiceCompleter
from ..lexertoken import LexerToken


class DummyLang(CompleterLang):
    # pylint: disable=super-init-not-called
    def __init__(self, expected_tokens, replaced_token=None, replaced_token_pos=-1):
        self._expected_tokens = expected_tokens
        self._replaced_token = replaced_token
        self._replaced_token_pos = replaced_token_pos

    def _construct_lexer(self):
        pass

    def get_expected_tokens(self, text, drop_last_token):
        if self._replaced_token is not None:
            replaced_token = LexerToken(type=self._expected_tokens,
                                        value=self._replaced_token,
                                        lexpos=self._replaced_token_pos)
        else:
            replaced_token = None
        return (self._expected_tokens, replaced_token, self._replaced_token_pos)


class TestLangCompleter(CompleterTestCase):
    default_expectation = "1ST_TOKEN"
    default_completers = {
        '1ST_TOKEN': ChoiceCompleter(("token1-a", "token1-b"), suffix=''),
        '2ND_TOKEN': ChoiceCompleter(("token2-a", "token2-b"), suffix=''),
        '3RD_TOKEN': ChoiceCompleter(("token3-a", "token3-b"), suffix=''),
        'PARTIAL_TOKEN': ChoiceCompleter(("tokenP-a", "tokenP-b"), prefix='@', suffix='')
    }

    # pylint: disable=arguments-differ,too-many-arguments
    def _construct_completer(self, expected_token=None, expected_tokens=None,
                             replaced_token=None, replaced_token_pos=-1,
                             completers=None, prefix="<!--"):
        if expected_tokens is None:
            expected_tokens = [expected_token or self.default_expectation]
        return LangBasedCompleter(parser=DummyLang(expected_tokens=expected_tokens,
                                                   replaced_token=replaced_token,
                                                   replaced_token_pos=replaced_token_pos),
                                  completers=completers or self.default_completers,
                                  prefix=prefix)

    def test_completing_on_the_first_characters_of_prefix_offers_the_prefix(self):
        self._assert_completions_offered("", expected_completions=["<!--"])
        self._assert_completions_offered("<", expected_completions=["<!--"])
        self._assert_completions_offered("<!", expected_completions=["<!--"])
        self._assert_completions_offered("<!-", expected_completions=["<!--"])

    def test_completing_on_an_input_with_a_mismatching_prefix(self):
        self._assert_no_completions_are_offered("mismatch")

    def test_completing_on_prefix_only_offers_the_completions_of_the_first_token_with_prefix_prepended(self):
        self._assert_completions_offered("<!--", expected_completions=["<!--token1-a", "<!--token1-b"])

    def test_completing_the_1st_empty_token_offers_completions_on_the_token(self):
        self._completer = self._construct_completer(expected_token="1ST_TOKEN")
        self._assert_completions_offered(entire_input="<!-- ", word="", expected_completions=["token1-a", "token1-b"])

    def test_completing_the_2nd_empty_token_offers_completions_on_the_token(self):
        self._completer = self._construct_completer(expected_token="2ND_TOKEN")
        self._assert_completions_offered(entire_input="<!-- foo ", word="",
                                         expected_completions=['token2-a', 'token2-b'])

    def test_completing_the_3rd_empty_token_offers_completions_on_the_token(self):
        self._completer = self._construct_completer(expected_token="3RD_TOKEN")
        self._assert_completions_offered(entire_input="<!-- foo bar ", word="",
                                         expected_completions=['token3-a', 'token3-b'])

    def test_completing_a_partial_token_with_an_empty_word_produces_choices_with_the_partial_prefix(self):
        self._completer = self._construct_completer(expected_token="PARTIAL_TOKEN")
        self._assert_completions_offered(entire_input="<!-- foo ", word="",
                                         expected_completions=['@'])

    def test_completing_a_partial_token_with_only_the_prefix_produces_choices_with_the_partial_prefix(self):
        # NOTE: replaced_token_pos is calculated after the language prefix (<!--) is chopped!
        self._completer = self._construct_completer(expected_token="PARTIAL_TOKEN",
                                                    replaced_token="@",
                                                    replaced_token_pos=5)
        self._assert_completions_offered(entire_input="<!-- foo @", word="@",
                                         expected_completions=['@tokenP-a', '@tokenP-b'])

    def test_completing_a_partial_token_with_a_few_matching_characters_produces_choices_with_the_partial_prefix(self):
        # NOTE: replaced_token_pos is calculated after the language prefix (<!--) is chopped!
        self._completer = self._construct_completer(expected_token="PARTIAL_TOKEN",
                                                    replaced_token="@tok",
                                                    replaced_token_pos=5)
        self._assert_completions_offered(entire_input="<!-- foo @tok", word="@tok",
                                         expected_completions=['@tokenP-a', '@tokenP-b'])

    def test_completing_a_partial_token_with_unmatching_characters_produces_no_completions(self):
        # NOTE: replaced_token_pos is calculated after the language prefix (<!--) is chopped!
        self._completer = self._construct_completer(expected_token="PARTIAL_TOKEN",
                                                    replaced_token="@unmatching",
                                                    replaced_token_pos=5)
        self._assert_no_completions_are_offered(entire_input="<!-- foo @unmatching", word="@unmatching")

    def test_completing_a_partial_token_that_is_longer_than_a_word_produces_no_matches(self):

        # This is pretty much a corner case, as this would mean that the word
        # splitting features of readline is not in-line with the tokenization
        # rules of the language. A sample is encoded below in the testcase,
        # the word itself is shorter than the token, which means that we
        # basically can't complete on the entire token as readline would only replace
        # stuff partially.
        #
        # The best we can do here is to offer no completions, the syntax we wanted
        # to extend didn't have such a case, fortunately.

        # NOTE: replaced_token_pos is calculated after the language prefix (<!--) is chopped!
        self._completer = self._construct_completer(expected_token="PARTIAL_TOKEN",
                                                    replaced_token="@token tail",
                                                    replaced_token_pos=5)
        self._assert_no_completions_are_offered(entire_input="<!-- foo @token tail", word="tail")

    def test_completing_a_token_where_multiple_tokens_could_match_collects_all_matches(self):
        self._completer = self._construct_completer(expected_tokens=["1ST_TOKEN", "2ND_TOKEN", "3RD_TOKEN"])
        self._assert_completions_offered(entire_input="<!-- ", word="",
                                         expected_completions=["token1-a", "token2-b", "token3-a"])
        self._assert_completions_not_offered(entire_input="<!-- ", word="",
                                             unexpected_completions=["@tokenP-a"])

    def test_completing_a_token_that_has_no_registered_completer_results_in_no_matches(self):
        self._completer = self._construct_completer(expected_token="NOSUCHCOMPLETER")
        self._assert_no_completions_are_offered("<!-- ")
