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

from .lexertoken import LexerToken
from .lexer import Lexer


class TabLexer(Lexer):
    def __init__(self, lexer):
        self._lexer = lexer
        self._current_token = None
        self._end_of_tokens = False
        self._replaced_token = None
        self._buffered_tokens = None
        self._buffer_count = 0
        self._is_last_token = False

    def input(self, text):
        self._lexer.input(text)
        self._end_of_tokens = False
        self._buffered_tokens = None
        self._replaced_token = None

    def get_replaced_token(self):
        return self._replaced_token

    def set_drop_last_token(self, value):
        if value:
            self._buffer_count = 1
        else:
            self._buffer_count = 0

    def token(self):
        if self._end_of_tokens:
            return None
        if not self._is_buffer_initialized():
            self._fill_buffer()

        self._shift_and_inject_tab()
        return self._current_token

    def get_position(self):
        # This method is not implemented as it would require to look ahead
        # to the next token we may or may not have, making the code more
        # difficult. We are not using this anyway.
        raise NotImplementedError

    def _shift_and_inject_tab(self):
        self._shift_from_buffer()

        if self._is_last_token or self._is_current_token_partial():
            self._end_of_tokens = True
            self._replaced_token = self._current_token
            self._current_token = self._constuct_tab_token()

    def _is_current_token_partial(self):
        return (self._current_token is not None and
                (hasattr(self._current_token, 'partial') and
                 self._current_token.partial))

    def _is_buffer_initialized(self):
        return self._buffered_tokens is not None

    def _constuct_tab_token(self):
        if self._replaced_token is not None:
            lexpos = self._replaced_token.lexpos
        else:
            lexpos = self._lexer.get_position()
        return LexerToken(type="TAB", lexpos=lexpos)

    def _shift_from_buffer(self):
        self._fetch_token_to_buffer()
        self._is_last_token = len(self._buffered_tokens) <= self._buffer_count
        self._current_token = self._get_token_from_buffer()

    def _fill_buffer(self):
        self._buffered_tokens = []
        for _ in range(self._buffer_count):
            self._fetch_token_to_buffer()

    def _fetch_token_to_buffer(self):
        token = self._lexer.token()
        if token is not None:
            self._buffered_tokens.append(token)

    def _get_token_from_buffer(self):
        try:
            token = self._buffered_tokens[0]
            self._buffered_tokens = self._buffered_tokens[1:]
            return token
        except IndexError:
            return None
