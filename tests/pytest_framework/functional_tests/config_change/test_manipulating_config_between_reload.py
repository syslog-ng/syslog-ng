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

from src.syslog_ng_tester import *

@with_file_source("file_source2", "input2.log")
@with_file_destination("file_destination2", "output2.log")
@with_file_source("file_source", "input.log")
@with_file_destination("file_destination", "output.log")
class ManipultingConfigBetweenReloadTester(SyslogNgTester):
    def __init__(self, testcase):
        SyslogNgTester.__init__(self, testcase)
        source_group = self.config.create_source_group(self.file_source)
        self.destination_group = self.config.create_destination_group(self.file_destination)
        self.logpath = self.config.create_logpath(sources=source_group, destinations=self.destination_group)

    def first_set_of_changes(self):
        self.__update_first_file_source_parameters()
        self.__update_logpaths()

    def second_set_of_changes(self):
        # remove option from file source
        self.file_source.options.pop("log_iw_size")

        # remove file destination from destination group
        self.destination_group.group_node.pop(self.file_destination2.driver_id)

        # remove second source group from logpath
        self.logpath.logpath_node["sources"].remove(self.source_group2.group_id)

    def __update_first_file_source_parameters(self):
        # update positional value of file source
        self.file_source.options["file_name"] = "updated_input.log"

        # add new option to file source
        self.file_source.options["log_iw_size"] = "100"

    def __update_logpaths(self):
        # add new file source to a separate source group
        self.source_group2 = self.config.create_source_group(self.file_source2)

        # update first destination group with a new file destination
        self.destination_group.update_group_node(self.file_destination2)

        # update first logpath group with new source group
        self.logpath.add_source_groups(self.source_group2)

def test_manipulating_config_between_reload(tc):

    tester = ManipultingConfigBetweenReloadTester(tc)
    tester.start()

    tester.first_set_of_changes()
    tester.reload()
    tester.second_set_of_changes()
    tester.reload()
