#!/usr/bin/env python
#############################################################################
# Copyright (c) 2020 One Identity
# Copyright (c) 2021 Xiaoyu Qiu
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


test_parameters_raw = [
    ("foo", '''""''', '''"(?<key>foo)"''', "", "${key}", True, True, "foo"),
    ("foo", '''""''', '''"(?<key>fo*)"''', "", "${key}", True, True, "foo"),
    ("foo", '''".reg."''', '''"(?<key>foo)"''', "", "${.reg.key}", True, True, "foo"),
    ("foo", '''".reg."''', '''"(?<key>foo)"''', "", "${key}", True, True, ""),
    ("foo", '''".reg."''', '''"(?<key>foo)|(?<key>bar)"''', "dupnames", "${.reg.key}", True, True, "foo"),
    ("abc", '''""''', '''"(?<key>Abc)"''', "", "${key}", True, False, ""),
    ("abc", '''""''', '''"(?<key>Abc)"''', "ignore-case", "${key}", True, True, "abc"),
    ("foobar", '''".reg."''', '''"(?<key>foo)", "(?<key>bar)"''', "", "${.reg.key}", True, True, "foo"),
    ("foo", '''""''', '''"(?<key>foo"''', "", "${key}", False, False, ""),
    ("foo", '''""''', '''"fo*"''', "", "$MSG", True, True, "foo"),
]


@pytest.mark.parametrize(
    "input_message, prefix, patterns, flags, template, compile_result, expected_result, expected_value", test_parameters_raw,
    ids=[
        "match_literally", "match_regular_expression",
        "match_with_prefix", "match_with_prefix_and_wrong_template", "match_with_dupnames", "unmatch_case",
        "match_ignore_case_flag", "match_multiple_regular_expressions", "regular_expression_compile_error",
        "check_MSG_macro",
    ],
)
def test_regexp_parser(config, syslog_ng, input_message, prefix, patterns, flags, template, compile_result, expected_result, expected_value):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    regexp_parser = config.create_regexp_parser(prefix=prefix, patterns=patterns, flags=flags)

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template + "\n"))
    config.create_logpath(statements=[generator_source, regexp_parser, file_destination])

    if compile_result:
        syslog_ng.start(config)
        if expected_result:
            assert file_destination.read_log().strip() == expected_value
            assert regexp_parser.get_query() == {'discarded': 0}
        else:
            assert regexp_parser.get_query() == {'discarded': 1}
    else:
        with pytest.raises(Exception):
            syslog_ng.start(config)
