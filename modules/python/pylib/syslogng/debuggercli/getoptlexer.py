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
