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


class ChoiceCompleter(Completer):
    def __init__(self, choices, prefix=None, suffix=None):
        self._choices = choices
        self._prefix = prefix or ''
        self._suffix = suffix if suffix is not None else ' '

    def complete(self, entire_text, word_to_be_completed):
        if entire_text.startswith(self._prefix):
            return self._handle_input_with_prefix(entire_text, word_to_be_completed)
        return self._handle_input_without_prefix(entire_text)

    def _handle_input_without_prefix(self, entire_input):
        if self._prefix.startswith(entire_input) or entire_input == '':
            return [self._prefix]
        else:
            return []

    def _handle_input_with_prefix(self, entire_input, word_to_be_completed):
        entire_input, word_to_be_completed = self._chop_prefixes(entire_input, word_to_be_completed)

        return sorted([self._prefix + choice + self._suffix
                       for choice in self._choices
                       if len(word_to_be_completed) == 0 or choice.startswith(word_to_be_completed)])

    def _chop_prefixes(self, entire_input, word_to_be_completed):
        if word_to_be_completed == entire_input:
            word_to_be_completed = word_to_be_completed[len(self._prefix):]
        entire_input = entire_input[len(self._prefix):]
        return entire_input, word_to_be_completed
