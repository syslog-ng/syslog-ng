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

import pytest

from utils.OptionParser import (_find_options, _find_options_with_keyword,
                                _find_options_wo_keyword, _get_resolve_db,
                                _parse_keyword_and_arguments, _parse_parents,
                                _resolve_context_token, _resolve_option,
                                _resolve_tokens, _sanitize)


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'", {(3, 7)}),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' string LL_IDENTIFIER '(' argument ')' ')'", {(4, 7)}),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_PARENT_BLOCK '(' KW_OPTION '(' argument ')' ')' ')'", {(6, 9)}),
        ("LL_CONTEXT_DESTINATION KW_NETWORK '(' string KW_FAILOVER '(' KW_SERVERS '(' string_list ')' KW_FAILBACK "
         "'(' KW_TCP_PROBE_INTERVAL '(' positive_integer ')' ')' ')' ')'", {(6, 9), (12, 15)})
    ]
)
def test_find_options_with_keyword(path, options):
    assert _find_options_with_keyword(path.split()) == options


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'", set()),
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' string LL_IDENTIFIER '(' argument ')' ')'", {(3, 3)}),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_PARENT_BLOCK '(' string number KW_OPTION '(' argument ')' ')' ')'",
         {(3, 3), (6, 7)}),
    ]
)
def test_find_options_wo_keyword(path, options):
    assert _find_options_wo_keyword(path.split()) == options


@pytest.mark.parametrize(
    'path,options',
    [
        ("LL_CONTEXT_DESTINATION KW_DRIVER '(' ')'", set()),
        ("LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_OPTION1 '(' argument argument ')' KW_PARENT_BLOCK '(' "
         "string number KW_OPTION '(' argument ')' ')' ')'", {(3, 3), (4, 8), (11, 12), (13, 16)}),
    ]
)
def test_find_options(path, options):
    assert _find_options(path.split()) == options


@pytest.mark.parametrize(
    'path,option_interval,option',
    [
        (
            "LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'",
            (3, 7),
            ('KW_OPTION', ('argument1', 'argument2'))
        ),
        (
            "LL_CONTEXT_DESTINATION KW_DRIVER '(' string LL_IDENTIFIER '(' argument ')' ')'",
            (3, 3),
            ('', ('string',))
        ),
        (
            "LL_CONTEXT_DESTINATION KW_NETWORK '(' string KW_FAILOVER '(' KW_SERVERS '(' string_list ')' "
            "KW_FAILBACK '(' KW_TCP_PROBE_INTERVAL '(' positive_integer ')' ')' ')' ')'",
            (12, 15),
            ('KW_TCP_PROBE_INTERVAL', ('positive_integer',))
        )
    ]
)
def test_parse_keyword_and_arguments(path, option_interval, option):
    assert _parse_keyword_and_arguments(path.split(), option_interval) == option


@pytest.mark.parametrize(
    'path,option_interval,parents',
    [
        (
            "LL_CONTEXT_DESTINATION KW_DRIVER '(' KW_OPTION '(' argument1 argument2 ')' ')'",
            (3, 7),
            ()
        ),
        (
            "LL_CONTEXT_SOURCE KW_DRIVER '(' string KW_PARENT_BLOCK '(' KW_OPTION '(' argument ')' ')' ')'",
            (6, 9),
            ('KW_PARENT_BLOCK',)
        ),
        (
            "LL_CONTEXT_DESTINATION KW_NETWORK '(' string KW_FAILOVER '(' KW_SERVERS '(' string_list ')' KW_FAILBACK '(' KW_TCP_PROBE_INTERVAL '(' positive_integer ')' ')' ')' ')'",
            (12, 15),
            ('KW_FAILOVER', 'KW_FAILBACK')
        )
    ]
)
def test_parse_parents(path, option_interval, parents):
    assert _parse_parents(path.split(), option_interval) == parents


@pytest.mark.parametrize(
    'token,sanitized',
    [
        ('"peer_verify"', 'peer-verify'),
        ("':'", ':')
    ]
)
def test_sanitize(token, sanitized):
    assert _sanitize(token) == sanitized


def test_resolve_context_token():
    assert _resolve_context_token('LL_CONTEXT_INNER_SRC') == 'inner-src'


def test_get_resolve_db():
    assert len(_get_resolve_db()) > 0


@pytest.mark.parametrize(
    'tokens,resolved_tokens',
    [
        (('KW_PEER_VERIFY',), ('peer-verify',)),
        (('KW_MARK_FREQ', 'LL_NUMBER'), ('mark/mark-freq', '<number>')),
        (('string', 'nonnegative_integer', 'KW_KEY', ''), ('<string>', '<nonnegative-integer>', 'key', ''))
    ]
)
def test_resolve_tokens(tokens, resolved_tokens):
    assert _resolve_tokens(tokens) == resolved_tokens


def test_resolve_tokens_keyword_without_resolvation():
    with pytest.raises(Exception) as e:
        _resolve_tokens(('KW_I_HAVE_NO_RESOLVE',))
    assert 'Keyword without resolvation:' in str(e.value)


@pytest.mark.parametrize(
    'option,resolved_option',
    [
        (
            (
                'LL_CONTEXT_SOURCE',
                'KW_WILDCARD_FILE',
                'KW_PAD_SIZE',
                ('nonnegative_integer',),
                ()
            ),
            (
                'source',
                'wildcard-file',
                'pad-size',
                ('<nonnegative-integer>',),
                ()
            ),
        ),
        (
            (
                'LL_CONTEXT_DESTINATION',
                'KW_RIEMANN',
                'LL_IDENTIFIER',
                ('template_content',),
                ('KW_ATTRIBUTES',)
            ),
            (
                'destination',
                'riemann',
                '<identifier>',
                ('<template-content>',),
                ('attributes',)
            )
        ),
        (
            (
                'LL_CONTEXT_DESTINATION',
                'KW_RIEMANN',
                '',
                ('KW_KEY', 'string'),
                ('KW_ATTRIBUTES',)
            ),
            (
                'destination',
                'riemann',
                '',
                ('key', '<string>'),
                ('attributes',)
            )
        )
    ]
)
def test_resolve_option(option, resolved_option):
    assert _resolve_option(*option) == resolved_option
