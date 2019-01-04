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


class LogPath(object):
    def __init__(self):
        self.__group_type = "log"
        self.__logpath = []

    @property
    def group_type(self):
        return self.__group_type

    @property
    def logpath(self):
        return self.__logpath

    def add_source_group(self, source_group):
        self.update_logpath_with_group(source_group)

    def add_source_groups(self, source_groups):
        self.update_logpath_with_groups(source_groups)

    def add_destination_group(self, destination_group):
        self.update_logpath_with_group(destination_group)

    def add_destination_groups(self, destination_groups):
        self.update_logpath_with_groups(destination_groups)

    def update_logpath_with_group(self, group):
        self.logpath.append(group)

    def update_logpath_with_groups(self, groups):
        for group in groups:
            self.update_logpath_with_group(group)
