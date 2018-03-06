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
from .completer import Completer


class LangBasedCompleter(Completer):

    def __init__(self, parser, completers, prefix=None):
        self._parser = parser
        self._completers = completers
        self._prefix = prefix or ''
        self._completing_at_the_beginning_of_the_grammar = False

    def complete(self, entire_text, word_to_be_completed):
        if entire_text.startswith(self._prefix):
            return self._handle_input_with_prefix(entire_text, word_to_be_completed)
        return self._handle_input_without_prefix(entire_text)

    def _handle_input_without_prefix(self, entire_input):
        if self._prefix.startswith(entire_input) or entire_input == '':
            return [self._prefix]
        return []

    def _handle_input_with_prefix(self, entire_input, word_to_be_completed):
        entire_input, word_to_be_completed = self._chop_prefixes(entire_input, word_to_be_completed)

        expected_tokens, last_token, completion_prefix, word_to_be_completed = (
            self._evaluate_language(entire_input, word_to_be_completed))

        completers = self._find_applicable_completers(expected_tokens)
        completions = self._collect_completions(completers, last_token, completion_prefix, word_to_be_completed)
        return completions

    def _chop_prefixes(self, entire_input, word_to_be_completed):
        if word_to_be_completed == entire_input:
            word_to_be_completed = word_to_be_completed[len(self._prefix):]
            self._completing_at_the_beginning_of_the_grammar = True
        else:
            self._completing_at_the_beginning_of_the_grammar = False
        entire_input = entire_input[len(self._prefix):]
        return entire_input, word_to_be_completed

    def _evaluate_language(self, entire_input, word_to_be_completed):
        expected_tokens, replaced_token, token_position = (
            self._parser.get_expected_tokens(entire_input, drop_last_token=(word_to_be_completed != '')))

        if token_position >= 0:
            word_position = len(entire_input) - len(word_to_be_completed)
            if word_position <= token_position:
                completion_prefix = entire_input[word_position:token_position]
                word_to_be_completed = entire_input[token_position:]
            else:
                completion_prefix = ''
        else:
            completion_prefix = ''

        if replaced_token is None:
            replaced_token = ''
        else:
            replaced_token = replaced_token.value or ''

        return expected_tokens, replaced_token, completion_prefix, word_to_be_completed

    def _find_applicable_completers(self, expected_tokens):
        completers = []
        for token in expected_tokens:
            try:
                completers.append(self._completers[token])
            except KeyError:
                pass
        return completers

    def _collect_completions(self, completers, last_token, completion_prefix, word_to_be_completed):
        if self._completing_at_the_beginning_of_the_grammar:
            prefix = self._prefix + completion_prefix
        else:
            prefix = completion_prefix
        completions = []
        for completer in completers:
            completions.extend(
                (prefix + completion for completion in completer.complete(last_token, word_to_be_completed)))
        completions = sorted(completions)
        return completions
