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

from utils.Yacc2Graph import yacc2graph


class BisonGraph():
    def __init__(self, yaccfile):
        with open(yaccfile, 'r') as f:
            yacc = f.read()
            self.graph = yacc2graph(yacc)

    def get_nodes(self):
        return list(self.graph.nodes)

    def _children_of_rule_sorted(self, node):
        children = []
        for child, arcs in self.graph[node].items():
            for _, arc in arcs.items():
                children.append((arc['index'], child))
        return [x[1] for x in sorted(children)]

    def get_children(self, node):
        if self.is_rule(node):
            return self._children_of_rule_sorted(node)
        else:
            return sorted(self.graph.successors(node))

    def get_parents(self, node):
        return sorted(self.graph.predecessors(node))

    def is_terminal(self, node):
        return len(list(self.graph.successors(node))) == 0

    def is_rule(self, node):
        if node not in self.get_nodes():
            raise Exception('Node not in graph: ' + node)
        try:
            int(node)
        except ValueError:
            return False
        return True

    def make_terminal(self, node):
        children = self.get_children(node)
        for child in children:
            self.graph.remove_edge(node, child)

    def remove(self, node):
        self.graph.remove_node(node)
