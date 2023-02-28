#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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


def test_sdata_parser(config, syslog_ng):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("[Originator@6876 sub=Vimsvc.ha-eventmgr opID=esxui-13c6-6b16 sid=5214bde6 user=root]"))
    sdata_parser = config.create_sdata_parser(prefix=config.stringify(".SDATA."))

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$SDATA\n"))
    config.create_logpath(statements=[generator_source, sdata_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log().strip() == '[Originator@6876 sub="Vimsvc.ha-eventmgr" opID="esxui-13c6-6b16" sid="5214bde6" user="root"]'
    assert sdata_parser.get_query().get('discarded', -1) == 0
