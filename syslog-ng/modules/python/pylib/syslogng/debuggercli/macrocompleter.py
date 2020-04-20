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
from .completer import Completer
from .syslognginternals import get_nv_registry


class MacroCompleter(Completer):
    _date_wildcards = {
        'R_': 'received time',
        'S_': 'stamp in message',
        'C_': 'current time'
    }

    def __init__(self, macros=None):
        super(MacroCompleter, self).__init__()
        self._macros = macros
        self._completions = []

    def complete(self, entire_text, word_to_be_completed):
        if not self._looks_like_a_macro_prefix(entire_text):
            return []
        self._collect_completions(word_to_be_completed)
        return sorted([completion
                       for completion in self._completions
                       if len(word_to_be_completed) == 0 or completion.startswith(word_to_be_completed)])

    def _looks_like_a_macro_prefix(self, entire_input):
        if entire_input == '':
            return True
        if entire_input == '$':
            return True
        if entire_input[0] == '$' and self._is_valid_macro(entire_input[1:]):
            return True
        if entire_input == '${':
            return True
        if entire_input[:2] == '${':
            return True
        return False

    def _is_valid_macro(self, macro):
        return all(self._is_valid_macro_char(c) for c in macro)

    @staticmethod
    def _is_valid_macro_char(macro_char):
        if macro_char >= 'A' and macro_char <= 'Z':
            return True
        if macro_char >= 'a' and macro_char <= 'z':
            return True
        if macro_char >= '0' and macro_char <= '9':
            return True
        if macro_char == '_':
            return True
        return False

    def _collect_completions(self, word):
        self._reset_completions()
        if word == '':
            self._add_completion('$')
            self._add_completion('${')
        elif word == '$':
            self._extend_completions(self._collect_unqualified_macros())
            self._extend_completions(self._collect_qualified_macros_with_brace())
            self._extend_completions(self._collect_small_numbered_matches())
            self._extend_completions(self._collect_date_wildcards())
            self._add_completion('${')
        elif self._is_word_a_numbered_match_prefix(word):
            self._extend_completions(self._collect_all_numbered_matches())
        elif self._is_word_a_nonbraced_prefix(word):
            if self._is_word_a_date_prefix(word):
                self._extend_completions(self._collect_date_macros())
            else:
                self._extend_completions(self._collect_unqualified_macros())
                self._extend_completions(self._collect_date_wildcards())
        elif word == '${':
            self._extend_completions(self._collect_small_numbered_matches_with_brace())
            self._extend_completions(self._collect_all_macros_with_brace())
            self._extend_completions(self._collect_date_wildcard_with_brace())
        elif self._is_word_a_numbered_match_prefix_with_brace(word):
            self._extend_completions(self._collect_all_numbered_matches_with_brace())
        elif self._is_word_a_braced_prefix(word):
            if self._is_word_a_date_prefix_with_brace(word):
                self._extend_completions(self._collect_date_macros_with_brace())
            else:
                self._extend_completions(self._collect_all_macros_with_brace())
                self._extend_completions(self._collect_date_wildcard_with_brace())

        return self._completions

    def _is_word_a_date_prefix(self, word):
        return word[:3] in ['$' + x for x in self._date_wildcards]

    @staticmethod
    def _is_word_a_numbered_match_prefix(word):
        return word[0] == '$' and word[1:2].isdigit()

    @staticmethod
    def _is_word_a_nonbraced_prefix(word):
        return word[0] == '$' and word[1] != '{'

    @staticmethod
    def _is_word_a_numbered_match_prefix_with_brace(word):
        return word[0:2] == '${' and word[2:3].isdigit()

    def _is_word_a_date_prefix_with_brace(self, word):
        return word[:4] in ['${' + x for x in self._date_wildcards]

    @staticmethod
    def _is_word_a_braced_prefix(word):
        return word[0:2] == '${'

    @staticmethod
    def _is_macro_qualified(macro):
        return '.' in macro

    @classmethod
    def _is_macro_unqualified(cls, macro):
        return (not cls._is_macro_qualified(macro) and
                not cls._is_macro_a_numbered_match(macro) and
                not cls._is_macro_a_date_macro(macro))

    @staticmethod
    def _is_macro_a_numbered_match(macro):
        return macro.isdigit()

    @classmethod
    def _is_macro_a_date_macro(cls, macro):
        return macro[0:2] in cls._date_wildcards

    @classmethod
    def _is_macro_a_small_numbered_match(cls, macro):
        return cls._is_macro_a_numbered_match(macro) and int(macro) < 10

    def _collect_macros_generic(self, predicate, template):
        for macro in self._macros or get_nv_registry():
            if predicate(macro):
                yield template.format(macro)

    def _collect_macros(self, predicate):
        return self._collect_macros_generic(predicate, '${}')

    def _collect_macros_with_brace(self, predicate):
        return self._collect_macros_generic(predicate, '${{{}}}')

    def _collect_unqualified_macros(self):
        return self._collect_macros(self._is_macro_unqualified)

    def _collect_qualified_macros_with_brace(self):
        return self._collect_macros_with_brace(self._is_macro_qualified)

    def _collect_date_macros(self):
        return self._collect_macros(self._is_macro_a_date_macro)

    def _collect_date_macros_with_brace(self):
        return self._collect_macros_with_brace(self._is_macro_a_date_macro)

    def _collect_small_numbered_matches(self):
        return self._collect_macros(self._is_macro_a_small_numbered_match)

    def _collect_small_numbered_matches_with_brace(self):
        return self._collect_macros_with_brace(self._is_macro_a_small_numbered_match)

    def _collect_all_numbered_matches(self):
        return self._collect_macros(self._is_macro_a_numbered_match)

    def _collect_all_numbered_matches_with_brace(self):
        return self._collect_macros_with_brace(self._is_macro_a_numbered_match)

    def _collect_all_macros_with_brace(self):
        return self._collect_macros_with_brace(
            lambda macro: (not self._is_macro_a_numbered_match(macro) and
                           not self._is_macro_a_date_macro(macro)))

    def _collect_date_wildcards(self):
        for (wildcard, description) in self._date_wildcards.items():
            yield '${}* ({})'.format(wildcard, description)

    def _collect_date_wildcard_with_brace(self):
        for (wildcard, description) in self._date_wildcards.items():
            yield '${{{}*}} ({})'.format(wildcard, description)

    def _reset_completions(self):
        self._completions = []

    def _extend_completions(self, iterable):
        self._completions.extend(iterable)

    def _add_completion(self, value):
        self._completions.append(value)
