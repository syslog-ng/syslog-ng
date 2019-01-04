#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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

from src.common.random_id import RandomId


class LogPath(object):
    def __init__(self):
        self.logpath_id = "logpath_%s" % RandomId(use_static_seed=False).get_unique_id()
        self.full_logpath_node = {self.logpath_id: {"sources": [], "destinations": []}}
        self.logpath_node = self.full_logpath_node[self.logpath_id]

    def add_source_groups(self, source_groups):
        target_node = self.logpath_node["sources"]
        self.update_logpath_node(target_node, source_groups)

    def add_destination_groups(self, destination_groups):
        target_node = self.logpath_node["destinations"]
        self.update_logpath_node(target_node, destination_groups)

    @staticmethod
    def update_logpath_node(target_node, config_groups):
        if isinstance(config_groups, list):
            for group in config_groups:
                target_node.append(group)
        else:
            group = config_groups
            target_node.append(group)
