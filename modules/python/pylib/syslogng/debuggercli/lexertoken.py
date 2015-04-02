from __future__ import print_function, absolute_import
import ply.lex


class LexerToken(ply.lex.LexToken):
    # pylint: disable=redefined-builtin
    def __init__(self, type, value=None, partial=False, lexpos=-1):
        self.type = type
        self.value = value
        self.partial = partial
        self.lineno = 0
        self.lexpos = lexpos
