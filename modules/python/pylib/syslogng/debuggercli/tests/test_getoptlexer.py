from __future__ import absolute_import, print_function
from .test_lexer import TestLexer
from ..getoptlexer import GetoptLexer
from ..commandlinelexer import CommandLineLexer


class TestGetoptLexer(TestLexer):
    def setUp(self):
        self._known_commands = ['print', 'foo-bar']
        self._aliases = {
            'p': 'print',
            'fb': 'foo-bar'
        }
        self._lexer = self._construct_lexer()

    # pylint: disable=arguments-differ
    def _construct_lexer(self, known_commands=None, known_options=None, aliases=None):
        return GetoptLexer(CommandLineLexer(),
                           known_commands=known_commands or self._known_commands,
                           known_options=known_options,
                           aliases=aliases or self._aliases)

    def test_lexer_returns_command_token_for_an_unknown_command_as_the_first_token(self):
        self._lexer.input("unknown-cmd")
        token = self._next_token()
        self.assertEquals(token.type, "COMMAND")

    def test_lexer_returns_specific_token_for_known_commands(self):
        for command in self._known_commands:
            self._lexer.input(command)
            self._assert_next_token_type_equals('COMMAND_{}'.format(command.upper().replace('-', '_')))

    def test_lexer_translates_aliases(self):
        for (alias, command) in self._aliases.items():
            self._lexer.input(alias)
            self._assert_next_token_type_equals('COMMAND_{}'.format(command.upper().replace('-', '_')))

    def test_known_commands_are_not_returned_as_tokens_for_non_first_arguments(self):
        self._lexer.input("print print")
        self.assertEquals(self._next_token().type, "COMMAND_PRINT")
        self.assertEquals(self._next_token().type, "ARG")

    def test_double_quoted_arguments_are_returned_as_a_single_token(self):
        self._lexer.input('print "foo bar"')
        self.assertEquals(self._next_token().type, "COMMAND_PRINT")
        token = self._next_token()
        self.assertEquals(token.type, "ARG")
        self.assertEquals(token.value, "foo bar")

    def test_lexer_returns_token_in_a_sequence(self):
        self._lexer.input('''print foo bar''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals('foo')
        self._assert_next_arg_equals('bar')
        self._assert_last_token_reached()

    def test_lexer_handles_quoted_args_by_unqouting_them(self):
        self._lexer.input('''print "foo bar" 'bar foo' ''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals('foo bar')
        self._assert_next_arg_equals('bar foo')
        self._assert_last_token_reached()

    def test_lexer_handles_parenthesized_expression_as_a_single_token(self):
        self._lexer.input('''print (''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals("(")

        self._lexer.input('''print ( )''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals("( )")

        self._lexer.input('''print (1 + 1)''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals("(1 + 1)")

        self._lexer.input('''print ) )''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals(")")
        self._assert_next_arg_equals(")")

        self._lexer.input('''print ($MSG == foobar)''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_arg_equals("($MSG == foobar)")

    def test_lexer_translates_args_to_option_tokens_if_they_are_known(self):
        self._lexer = self._construct_lexer(known_options=("--foo", "--bar", "-a", "-b", "abc"))
        self._lexer.input('''print --foo --bar -a -b abc''')
        self._assert_next_token_type_equals("COMMAND_PRINT")
        self._assert_next_token_type_equals("OPT__FOO")
        self._assert_next_token_type_equals("OPT__BAR")
        self._assert_next_token_type_equals("OPT_A")
        self._assert_next_token_type_equals("OPT_B")
        self._assert_next_token_type_equals("OPTABC")

    def test_lexer_doesnt_translate_command_token_as_an_option(self):
        self._lexer = self._construct_lexer(known_options=("--foo", "--bar", "-a", "-b"))
        self._lexer.input('''--foo --bar -a -b''')
        self._assert_next_token_equals("COMMAND", value="--foo")

    def _assert_last_token_reached(self):
        token = self._next_token()
        self.assertIsNone(token)

    def _assert_next_arg_equals(self, token_value):
        token = self._next_token()
        self.assertEquals(token.type, "ARG")
        self.assertEquals(token.value, token_value)
