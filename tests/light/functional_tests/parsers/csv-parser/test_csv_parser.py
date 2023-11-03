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
    {
        "id": "simple",
        "input_message": "foo,bar",
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "quoted",
        "input_message": '''foo,"bar"''',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "simple-quotes",
        "quotes": "~^",
        "input_message": '''~foo~,^bar^''',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "quote-pairs",
        "quote-pairs": "><~~",
        "input_message": '''~foo~,>bar<''',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "escape-none",
        "input_message": "foo,bar",
        "dialect": "escape-none",
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "escape-double-char",
        "input_message": '''foo,"b""a""r"''',
        "dialect": "escape-double-char",
        "expected_value": 'foo=foo bar=b"a"r',
    },
    {
        "id": "escape-backslash",
        "input_message": r'''foo,"b\"a\"r\a"''',
        "dialect": "escape-backslash",
        "expected_value": 'foo=foo bar=b"a"ra',
    },
    {
        "id": "escape-backslash-with-sequences",
        "input_message": r'''foo,"b\"a\"r\a"''',
        "dialect": "escape-backslash-with-sequences",
        "expected_value": 'foo=foo bar=b"a"r\a',
    },
    {
        "id": "null-value",
        "input_message": "foo,NULL",
        "null": "NULL",
        "expected_value": "foo=foo bar=>>unset<<",
    },
    {
        "id": "delimiter-character",
        "input_message": "foo^bar",
        "delimiters": '"^"',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "delimiter-character2",
        "input_message": "foo^bar",
        "delimiters": 'chars("^")',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "delimiter-string",
        "input_message": "foo^^^bar",
        "delimiters": 'strings("^^^", "~~~")',
        "expected_value": "foo=foo bar=bar",
    },
    {
        "id": "delimiter-string2",
        "input_message": "foo~~~bar",
        "delimiters": 'strings("^^^", "~~~")',
        "expected_value": "foo=foo bar=bar",
    },
]


def add_optional_arg(config, args, testcase, key):
    if key in testcase:
        args[key] = testcase[key]


def add_optional_string_arg(config, args, testcase, key):
    if key in testcase:
        args[key] = config.stringify(testcase[key])


@pytest.mark.parametrize(
    "testcase", test_parameters_raw,
    ids=map(lambda tc: tc['id'], test_parameters_raw),
)
def test_csv_parser(config, syslog_ng, testcase):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(testcase["input_message"]))
    csv_parser_args = {}

    csv_parser_args['delimiters'] = config.stringify(',')
    add_optional_arg(config, csv_parser_args, testcase, 'dialect')
    add_optional_string_arg(config, csv_parser_args, testcase, 'quotes')
    add_optional_string_arg(config, csv_parser_args, testcase, 'quote-pairs')
    add_optional_string_arg(config, csv_parser_args, testcase, 'null')
    add_optional_arg(config, csv_parser_args, testcase, 'delimiters')
    csv_parser = config.create_csv_parser(prefix=config.stringify("prefix."), columns='''"foo", "bar"''', **csv_parser_args)

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("foo=${prefix.foo:->>unset<<} bar=${prefix.bar:->>unset<<}\n"))
    config.create_logpath(statements=[generator_source, csv_parser, file_destination])

    syslog_ng.start(config)

    assert file_destination.read_log().strip() == testcase['expected_value']
    assert csv_parser.get_query().get('discarded', -1) == 0


def test_csv_parser_into_matches(config, syslog_ng):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("FOO,BAR,BAZ"))

    csv_parser = config.create_csv_parser(delimiters=config.stringify(","))

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$*\n"))
    config.create_logpath(statements=[generator_source, csv_parser, file_destination])

    syslog_ng.start(config)

    assert file_destination.read_log().strip() == "FOO,BAR,BAZ"
    assert csv_parser.get_query().get('discarded', -1) == 0


test_parameters_drop_invalid = [
    {
        "id": "too_many_columns_in_input",
        "input_message": "foo,bar,baz",
        "columns": "foo,bar",
    },
    {
        "id": "type_does_not_match",
        "input_message": "foo,bar",
        "columns": "int(foo),bar",
    },
]


@pytest.mark.parametrize(
    "testcase", test_parameters_drop_invalid,
    ids=map(lambda tc: tc['id'], test_parameters_drop_invalid),
)
def test_csv_parser_drop_invalid(config, syslog_ng, testcase):
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(testcase["input_message"]))

    csv_parser = config.create_csv_parser(delimiters=config.stringify(","), drop_invalid="yes", columns=testcase["columns"])

    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$*\n"))
    config.create_logpath(statements=[generator_source, csv_parser, file_destination])

    syslog_ng.start(config)

    assert csv_parser.get_query().get('discarded', -1) == 1
