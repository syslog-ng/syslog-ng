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
from .completerlang import CompleterLang
from .commandlinelexer import CommandLineLexer
from .getoptlexer import GetoptLexer


class TemplateFunctionLang(CompleterLang):
    tokens = []
    tokens_base = [
        "COMMAND", "ARG", "OPT"
    ]
    known_commands = ("format-json", "echo")
    known_options = (
        # value-pairs
        "--scope", "--exclude", "--key", "--rekey", "--pair",
        "--shift", "--add-prefix", "--replace-prefix", "--replace")

    def p_template_func(self, p):
        '''template_func : tf_format_json
                         | tf_echo
                         | tf_generic'''
        pass

    def p_tf_format_json(self, p):
        '''tf_format_json : COMMAND_FORMAT_JSON tf_format_json_args'''
        pass

    def p_tf_format_json_args(self, p):
        '''tf_format_json_args : tf_format_json_arg tf_format_json_args
                               | '''
        pass

    def p_tf_format_json_arg(self, p):
        '''tf_format_json_arg  : OPT__SCOPE value_pairs_scope
                               | OPT__EXCLUDE ARG
                               | OPT__KEY name_value_name
                               | OPT__REKEY ARG
                               | OPT__PAIR name_value_pair
                               | OPT__SHIFT ARG
                               | OPT__ADD_PREFIX ARG
                               | OPT__REPLACE_PREFIX ARG
                               | OPT__REPLACE ARG
                               | OPT
                               | ARG'''
        pass

    def p_name_value_name(self, p):
        '''name_value_name : ARG'''
        pass

    def p_name_value_pair(self, p):
        '''name_value_pair : ARG'''
        pass

    def p_value_pairs_scope(self, p):
        '''value_pairs_scope : ARG'''
        pass

    def p_tf_echo(self, p):
        '''tf_echo : COMMAND_ECHO templates'''
        pass

    def p_tf_generic(self, p):
        '''tf_generic : COMMAND args'''
        pass

    def p_args(self, p):
        '''args : ARG args
                |
        '''
        pass

    def p_templates(self, p):
        '''templates : template templates
                     | '''
        pass

    def p_template(self, p):
        '''template : ARG'''
        pass

    def p_ipaddress(self, p):
        '''ipaddress : ARG'''
        pass

    def _initialize_rules(self):
        tokens = list(self.tokens_base)
        self._convert_known_commands_to_tokens(tokens)
        self._convert_known_options_to_tokens(tokens)
        self.tokens = tokens

    def _construct_lexer(self):
        return GetoptLexer(CommandLineLexer(),
                           known_commands=self.known_commands,
                           known_options=self.known_options)

    @staticmethod
    def _tokenize_argument(arg):
        return arg.upper().replace('-', '_')

    def _convert_known_commands_to_tokens(self, tokens):
        for command in self.known_commands:
            tokens.append("COMMAND_{}".format(self._tokenize_argument(command)))

    def _convert_known_options_to_tokens(self, tokens):
        for option in self.known_options:
            tokens.append("OPT{}".format(self._tokenize_argument(option)))
