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
from .lexer import Lexer


class GetoptLexer(Lexer):
    def __init__(self, lexer, known_commands, known_options=None, aliases=None):
        self._lexer = lexer
        self._current_token = 0
        self._known_commands = known_commands
        self._known_options = known_options or {}
        self._aliases = aliases or {}

    def input(self, text):
        self._lexer.input(text)
        self._current_token = 0

    def token(self):
        token = self._lexer.token()
        if token is not None:
            if self._current_token == 0:
                translated_token = self._translate_alias(token.value)
                if translated_token in self._known_commands:
                    token.type = 'COMMAND_{}'.format(translated_token.upper().replace('-', '_'))
                else:
                    token.type = 'COMMAND'
            else:
                if token.value in self._known_options:
                    token.type = "OPT{}".format(token.value.upper().replace('-', '_'))
            self._current_token += 1
        return token

    def get_position(self):
        return self._lexer.get_position()

    def _translate_alias(self, token):
        return self._aliases.get(token, token)
