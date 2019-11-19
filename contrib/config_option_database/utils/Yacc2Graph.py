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

import xml.etree.ElementTree as xml_parser
from os import remove
from subprocess import DEVNULL, Popen
from tempfile import NamedTemporaryFile

import networkx


class Rule():
    def __init__(self, number, parent, symbols):
        self.number = number
        self.parent = parent
        self.symbols = symbols


def _run_in_shell(command):
    proc = Popen(command, stderr=DEVNULL, stdout=DEVNULL)
    proc.wait()
    return proc.returncode == 0


def _write_to_file(content):
    file = NamedTemporaryFile(mode='w')
    file.write(content)
    file.flush()
    file.seek(0)
    return file


def _yacc2xml(yacc_content):
    with _write_to_file(yacc_content) as file:
        xml_filepath = NamedTemporaryFile().name
        try:
            if not _run_in_shell(['bison', '--xml=' + xml_filepath, '--output=/dev/null', file.name]):
                raise Exception('Failed to convert to xml:\n{}\n'.format(yacc_content))
        except FileNotFoundError:
            raise Exception('bison executable not found')
        return xml_filepath


def _xml2rules(filename):
    rules = []
    root = xml_parser.parse(filename).getroot()
    for rule in root.iter('rule'):
        number = int(rule.get('number'))
        parent = rule.find('lhs').text
        symbols = [symbol.text for symbol in rule.find('rhs') if symbol.tag != 'empty']
        rules.append(Rule(number, parent, symbols))
    return rules


def _yacc2rules(yacc):
    xml = _yacc2xml(yacc)
    rules = _xml2rules(xml)
    remove(xml)
    return rules


def _rules2graph(rules):
    graph = networkx.MultiDiGraph()
    for rule in rules:
        rule_node = str(rule.number)
        graph.add_edge(rule.parent, rule_node)
        for index, symbol in enumerate(rule.symbols):
            graph.add_edge(rule_node, symbol, index=index)
    return graph


# yacc2graph:
# The structure of the graph is the following:
#  * The graph is directed
#  * A node can either be a symbol (terminal/nonterminal) or a rule number
#  * The edges connect the parent nodes to its children nodes
#  * Rule numbers always have symbols as children
#  * Symbols always have rule numbers as children
#  * Edges can be indexed
#  * When we are traversing through the graph, if a node's edges
#    are indexed, the edges must be traversed in ascending order,
#    if they are not indexed, the order does not matter
#  * Rule numbers' edges to their children are indexed
#  * Symbols' edges to their children are not indexed
#
# In simple words, take the following yacc syntax:
# %token test1
# %token test1next
# %token test2
# %token test2next
# %%
# start
#     : test                      # rule number 0
#     ;
# test
#     : test1 test1next test1next # rule number 1
#     | test2 test2next test      # rule number 2
#     ;
#
# * 'start' and 'test' are nonterminal symbols
# * test1, test1next, test2, test2next are terminal symbols
# * Every line, starting with a ':' or a '|' are rules, and they are numbered
#
# * The child of 'start' is 'rule number 0', and it is not indexed
# * The child of 'rule number 0' is 'test', and it is indexed
# * The children of 'test' are 'rule number 0' and 'rule number 1', and they are not indexed
# * The children of 'rule number 1' are 'test1', 'test1next' and 'test1next' and they are indexed
# * The children of 'rule number 2' are 'test2', 'test2next' and 'test', and they are indexed
#
# (See the unit tests for more examples)
def yacc2graph(yacc):
    return _rules2graph(_yacc2rules(yacc))
