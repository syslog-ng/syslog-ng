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


def test_manipulating_config_between_reload(tc):
    config = tc.new_config()

    file_source = config.create_file_source(file_name="input.log")
    source_group = config.create_source_group(file_source)

    file_destination = config.create_file_destination(file_name="output.log")
    destination_group = config.create_destination_group(file_destination)

    logpath = config.create_logpath(sources=source_group, destinations=destination_group)

    syslog_ng = tc.new_syslog_ng()
    syslog_ng.start(config)

    # update positional value of file source
    file_source.options["file_name"] = "updated_input.log"

    # add new option to file source
    file_source.options["log_iw_size"] = "100"

    # create new file source and add to separate source group
    file_source2 = config.create_file_source(file_name="input2.log")
    source_group2 = config.create_source_group(file_source2)

    # create new file destination and update first destination group
    file_destination2 = config.create_file_destination(file_name="output2.log")
    destination_group.update_group_with_statement(file_destination2)

    # update first logpath group with new source group
    logpath.add_source_groups(source_group2)

    syslog_ng.reload(config)

    # remove option from file source
    file_source.options.pop("log_iw_size")

    # remove file destination from destination group
    destination_group.remove_statement(file_destination2)

    # remove second source group from logpath
    logpath.logpath_node["sources"].remove(source_group2)

    syslog_ng.reload(config)
