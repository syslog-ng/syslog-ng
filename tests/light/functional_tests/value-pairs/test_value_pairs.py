#!/usr/bin/env python
#############################################################################
# Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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


test_parameters = [
    ("$(format-json test.*)\n", """{"test":{"key2":"value2","key1":"value1"}}\n"""),
    # transformations
    ("$(format-json test.* --add-prefix foo.)\n", """{"foo":{"test":{"key2":"value2","key1":"value1"}}}\n"""),
    ("$(format-json test.* --replace-prefix test=foobar)\n", """{"foobar":{"key2":"value2","key1":"value1"}}\n"""),
    ("$(format-json test.* --shift-levels 1)\n", """{"key2":"value2","key1":"value1"}\n"""),
    ("$(format-json test.* --shift 2)\n", """{"st":{"key2":"value2","key1":"value1"}}\n"""),
    ("$(format-json test.* --upper)\n", """{"TEST":{"KEY2":"value2","KEY1":"value1"}}\n"""),
    ("$(format-json MESSAGE --lower)\n", """{"message":"-- Generated message. --"}\n"""),
]


@pytest.mark.parametrize(
    "template, expected_value", test_parameters,
    ids=list(map(lambda x: x[0], test_parameters)),
)
def test_value_pairs(config, syslog_ng, template, expected_value):

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_log()
    assert log == expected_value
