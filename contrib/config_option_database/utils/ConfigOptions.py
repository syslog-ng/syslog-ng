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

from tempfile import NamedTemporaryFile

from utils.BisonGraph import BisonGraph
from utils.MergeYm import merge_grammars
from utils.OptionParser import path_to_options


def _make_types_terminal(graph):
    types = [
        'nonnegative_integer',
        'path',
        'positive_integer',
        'string',
        'string_list',
        'string_or_number',
        'template_content',
        'yesno'
    ]
    helpers = [
        'filter_content',
        'parser_content'
    ]

    for node in types + helpers:
        graph.make_terminal(node)


def _remove_code_blocks(graph):
    for node in filter(lambda x: x.startswith('$@'), graph.get_nodes()):
        graph.remove(node)


def _remove_ifdef(graph):
    nodes = ['KW_IFDEF', 'KW_ENDIF']
    for node in nodes:
        for parent in graph.get_parents(node):
            graph.remove(parent)


def get_driver_options():
    with NamedTemporaryFile(mode='w') as yaccfile:
        merge_grammars(yaccfile.name)
        graph = BisonGraph(yaccfile.name)
    _make_types_terminal(graph)
    _remove_code_blocks(graph)
    _remove_ifdef(graph)
    not_empty = filter(lambda path: len(path) > 0, graph.get_paths())
    paths = filter(lambda path: path[0] in ['LL_CONTEXT_SOURCE', 'LL_CONTEXT_DESTINATION', 'LL_CONTEXT_PARSER'], not_empty)
    options = set()
    for path in paths:
        options |= path_to_options(path)
    return options
