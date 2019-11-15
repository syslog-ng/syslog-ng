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
from tempfile import NamedTemporaryFile

from utils.BisonGraph import BisonGraph


@pytest.fixture
def graph():
    test_string = r"""
%token test1
%token test1next
%token test2
%token test2next
%token KW_TEST
%token number
%token string
%%
start
    : test
    ;
test
    : test1 test1next test1next
    | test2 test2next test
    | KW_TEST '(' test_opts ')'
      {
        int dummy_variable;
        some_c_code(dummy_variable);
      }
    |
    ;
test_opts
    : number
    | string
    ;
%%
"""
    with NamedTemporaryFile(mode='w') as f:
        f.write(test_string)
        f.flush()
        f.seek(0)
        graph = BisonGraph(f.name)
    return graph


def test_get_node(graph):
    expected = [
        '$accept', '0', 'start', '1', 'test', '2', '3', '4', 'test_opts',
        '6', '7', '$end', 'test1', 'test1next', 'test2', 'test2next',
        'KW_TEST', "'('", "')'", 'number', 'string', '5'
    ]
    assert sorted(graph.get_nodes()) == sorted(expected)


@pytest.mark.parametrize(
    'node,children,parents,is_rule,is_terminal',
    [
        ('$accept', ['0'], [], False, False),
        ('0', ['start', '$end'], ['$accept'], True, False),
        ('start', ['1'], ['0'], False, False),
        ('1', ['test'], ['start'], True, False),
        ('test', ['2', '3', '4', '5'], ['1', '3'], False, False),
        ('2', ['test1', 'test1next', 'test1next'], ['test'], True, False),
        ('3', ['test2', 'test2next', 'test'], ['test'], True, False),
        ('4', ['KW_TEST', "'('", 'test_opts', "')'"], ['test'], True, False),
        ('test_opts', ['6', '7'], ['4'], False, False),
        ('6', ['number'], ['test_opts'], True, False),
        ('7', ['string'], ['test_opts'], True, False),
        ('$end', [], ['0'], False, True),
        ('test1', [], ['2'], False, True),
        ('test1next', [], ['2'], False, True),
        ('test2', [], ['3'], False, True),
        ('test2next', [], ['3'], False, True),
        ('KW_TEST', [], ['4'], False, True),
        ("'('", [], ['4'], False, True),
        ("')'", [], ['4'], False, True),
        ('number', [], ['6'], False, True),
        ('string', [], ['7'], False, True),
        ('5', [], ['test'], True, True),
    ]
)
def test_children_junction_terminal(node, children, parents, is_rule, is_terminal, graph):
    assert graph.get_children(node) == children
    assert graph.get_parents(node) == parents
    assert graph.is_rule(node) == is_rule
    assert graph.is_terminal(node) == is_terminal


def test_is_rule_node_not_in_graph(graph):
    with pytest.raises(Exception) as e:
        graph.is_rule('invalid_node')
    assert 'Node not in graph:' in str(e.value)


def test_make_terminal(graph):
    assert graph.get_children('test_opts') != []
    graph.make_terminal('test_opts')
    assert graph.get_children('test_opts') == []
