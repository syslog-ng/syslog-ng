#############################################################################
# Copyright (c) 2019 Balabit
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

import re
from pathlib import Path

resolve_db = None


def _find_options_with_keyword(path):
    options = set()
    option_start = None
    for index, token in enumerate(path):
        if token == "'('" and index != 2:
            option_start = index - 1
        elif token == "')'" and option_start is not None:
            options.add((option_start, index))
            option_start = None
    return options


def _find_options_wo_keyword(path):
    options = set()
    left_brace = None
    for index, token in enumerate(path):
        if token == "'('":
            if left_brace is None:
                left_brace = index
                continue
            option_start = left_brace + 1
            option_end = index - 2
            if option_start <= option_end:
                options.add((option_start, option_end))
            left_brace = index
        elif token == "')'":
            left_brace = None
    return options


def _find_options(path):
    return _find_options_wo_keyword(path) | _find_options_with_keyword(path)


def _parse_keyword_and_arguments(path, option):
    tokens = tuple(path[option[0]:option[1] + 1])
    if "'('" in tokens and "')'" in tokens:
        assert tokens[1] == "'('" and tokens[-1] == "')'", tokens
        keyword = tokens[0]
        arguments = tokens[2:-1]
    else:
        keyword = ''
        arguments = tokens
    return keyword, arguments


def _parse_parents(path, option):
    parents = []
    skip = 0
    for index, token in reversed(list(enumerate(path[:option[0]]))):
        if token == "'('":
            if skip:
                skip -= 1
            else:
                parents.append(path[index - 1])
        elif token == "')'":
            skip += 1
    return tuple(reversed(parents[:-1]))


def _sanitize(string):
    return string.replace('"', '').replace("'", '').replace('_', '-').lower()


def _resolve_context_token(context):
    return _sanitize(context.replace('LL_CONTEXT_', ''))


def _get_resolve_db():
    # TODO: Fix reuse of a keyword in different drivers
    global resolve_db
    root_dir = Path(__file__).parents[3]
    if not resolve_db:
        resolve_db = {}
        struct_regex = re.compile(r'CfgLexerKeyword[^;]*')
        entry_regex = re.compile(r'{[^{}]+,[^{}]+}')
        for f in root_dir.rglob('*-parser.c'):
            for struct_match in struct_regex.finditer(f.read_text().replace('\n', '')):
                for entry_match in entry_regex.finditer(struct_match.group(0)):
                    entry = entry_match.group(0)[1:-1].replace(' ', '').split(',')
                    token = entry[1]
                    string = _sanitize(entry[0])
                    resolve_db.setdefault(token, set()).add(string)
    return resolve_db


def _resolve_token(token):
    if len(token) == 0:
        return token
    type_template = '<{}>'
    if token.startswith('LL_'):
        return type_template.format(_sanitize(token[3:]))
    try:
        db = _get_resolve_db()
        return '/'.join(sorted(db[token]))
    except KeyError:
        if token.startswith("KW_"):
            raise Exception('Keyword without resolvation: ' + token)
        return type_template.format(_sanitize(token))


def _resolve_tokens(tokens):
    resolved_tokens = tuple()
    for token in tokens:
        resolved_tokens += (_resolve_token(token),)
    return resolved_tokens
