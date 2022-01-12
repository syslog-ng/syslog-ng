#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 Balabit
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
from src.syslog_ng_config.statements.filters.filter import Match


def test_set_tag(config, syslog_ng):
    match = Match(config.stringify("MATCHSTRING"), value=config.stringify("MSG"))
    notmatch = Match(config.stringify("NONE"), value=config.stringify("MSG"))

    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("input with MATCHSTRING in it"))
    set_tag_with_matching = config.create_rewrite_set_tag("SHOULDMATCH", condition=match)
    set_tag_without_matching = config.create_rewrite_set_tag("DONOTMATCH", condition=notmatch)
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify('${TAGS}\n'))
    config.create_logpath(statements=[generator_source, set_tag_with_matching, set_tag_without_matching, file_destination])

    syslog_ng.start(config)
    log_line = file_destination.read_log().strip()
    assert "SHOULDMATCH" in log_line
    assert "DONOTMATCH" not in log_line
