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
import ply.lex as lex
from .lexertoken import LexerToken
from .lexer import Lexer


class LexBasedLexer(Lexer):
    def __init__(self):
        self._lexer = lex.lex(object=self)
        self._lexer.current_token = ''
        self._lexer.current_token_pos = -1

    def token(self):
        token = self._lexer.token()
        if token is None and self._lexer.lexstate != 'INITIAL':
            return LexerToken('LITERAL',
                              value=self._lexer.current_token,
                              partial=True,
                              lexpos=self._lexer.current_token_pos)
        return token

    def input(self, text):
        while len(self._lexer.lexstatestack) > 0:
            self._lexer.pop_state()
        return self._lexer.input(text)

    def get_position(self):
        return self._lexer.lexpos


class TemplateLexerError(Exception):
    pass


class TemplateLexer(LexBasedLexer):
    # pylint: disable=no-self-use,invalid-name
    tokens = (
        'LITERAL', 'MACRO', "TEMPLATE_FUNC"
    )

    states = (
        ('dollar', 'exclusive'),
        ('dollarbrace', 'exclusive'),
        ('dollarparen', 'exclusive'),
        ('string', 'exclusive'),
        ('qstring', 'exclusive'),
    )

    # Tokens

    def t_LITERAL(self, t):
        r'[^\$]+'
        t.type = "LITERAL"
        return t

    def t_DOLLAR(self, t):
        r'\$'
        t.lexer.push_state('dollar')
        t.lexer.current_token = '$'
        t.lexer.current_token_pos = t.lexpos

    def t_dollar_MACRO(self, t):
        r'[a-zA-Z0-9_]+'
        t.lexer.pop_state()
        t.value = '$' + t.value
        t.lexpos = t.lexer.current_token_pos
        return t

    def t_dollar_BRACE_OPEN(self, t):
        r'{'
        t.lexer.push_state('dollarbrace')
        t.lexer.current_token = '$' + t.value

    def t_dollarbrace_MACRO(self, t):
        r'[^}]+'
        t.lexer.current_token += t.value

    def t_dollarbrace_BRACE_CLOSE(self, t):
        r'}'
        t.lexer.current_token += t.value
        # go back to INITIAL
        t.lexer.pop_state()
        t.lexer.pop_state()
        t.type = 'MACRO'
        t.value = t.lexer.current_token
        t.lexpos = t.lexer.current_token_pos
        return t

    def t_dollar_PAREN_OPEN(self, t):
        r'\('
        t.lexer.push_state('dollarparen')
        t.lexer.paren_count = 1
        t.lexer.current_token = '$('
        t.lexer.current_token_pos = t.lexpos - 1

    def t_dollarparen_PAREN_OPEN(self, t):
        r'\('
        t.lexer.paren_count += 1
        t.lexer.current_token += '('

    def t_dollarparen_TEMPLATE_FUNC(self, t):
        r'''[^()'"]+'''
        t.lexer.current_token += t.value

    def t_dollarparen_PAREN_CLOSE(self, t):
        r'\)'
        t.lexer.current_token += ')'
        t.lexer.paren_count -= 1
        if t.lexer.paren_count == 0:
            t.type = 'TEMPLATE_FUNC'
            t.value = t.lexer.current_token
            t.lexpos = t.lexer.current_token_pos
            t.lexer.pop_state()
            t.lexer.pop_state()
            return t

        return None

    def t_dollarparen_QUOTE(self, t):
        r'"'
        t.lexer.current_token += t.value
        t.lexer.push_state('string')

    def t_dollarparen_APOSTROPHE(self, t):
        r"'"
        t.lexer.current_token += t.value
        t.lexer.push_state('qstring')

    def t_string_CHARS(self, t):
        r'[^"\\]'
        t.lexer.current_token += t.value

    def t_string_backslash_plus_character(self, t):
        r'\\.'
        t.lexer.current_token += t.value

    def t_string_QUOTE(self, t):
        r'"'

        # closing quote character
        t.lexer.current_token += t.value
        t.lexer.pop_state()

    def t_qstring_CHARS(self, t):
        r"[^']"
        t.lexer.current_token += t.value

    def t_qstring_APOSTROPHE(self, t):
        r"'"

        # closing apostrophe
        t.lexer.current_token += t.value
        t.lexer.pop_state()

    def t_error(self, t):
        raise TemplateLexerError("Illegal character {} in state {}".format(t.value, self._lexer.lexstate))

    def t_dollar_error(self, t):
        return self.t_error(t)

    def t_dollarbrace_error(self, t):
        return self.t_error(t)

    def t_dollarparen_error(self, t):
        return self.t_error(t)

    def t_string_error(self, t):
        return self.t_error(t)

    def t_qstring_error(self, t):
        return self.t_error(t)
