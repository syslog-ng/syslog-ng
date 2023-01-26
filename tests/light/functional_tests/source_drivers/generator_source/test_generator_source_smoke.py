#!/usr/bin/env python
#############################################################################
# Copyright (c) 2022 Andras Mitzki <mitzkia@gmail.com>
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

from functional_tests.parametrize_smoke_testcases import generate_id_name
from functional_tests.parametrize_smoke_testcases import generate_options_and_values_for_driver


@pytest.mark.parametrize("option_and_value", generate_options_and_values_for_driver("source", "example-msg-generator"), ids=generate_id_name)
def test_example_msg_generator_source_smoke(config, syslog_ng, option_and_value):

    config.update_global_options(stats_level=5)
    example_msg_generator_source = config.create_example_msg_generator_source(**option_and_value)

    file_destination = config.create_file_destination(file_name="output.log")
    config.create_logpath(statements=[example_msg_generator_source, file_destination])

    syslog_ng.start(config)
    syslog_ng.reload(config)

    assert file_destination.read_log() != ""
    assert file_destination.get_query()["written"] != 0
