#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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


test_parameters_first_match = [
    ("""foo foomessage""", "${.app.name}", "foo"),
    ("""bar barmessage""", "${.app.name}", "bar"),
    ("""foobar foobarmessage""", "${.app.name}", "foo"),
]


test_parameters_all_matches = [
    ("""foo foomessage""", "${FOOVALUE} ${BARVALUE}", "foo "),
    ("""bar barmessage""", "${FOOVALUE} ${BARVALUE}", " bar"),
    ("""foobar foobarmessage""", "${FOOVALUE} ${BARVALUE}", "foo bar"),
]


preamble_foobar_apps = """
application foo[syslog] {
    filter { program("foo"); };
    parser {
        channel {
            rewrite { set("foo" value("FOOVALUE"));  };
        };
    };
};

application bar[syslog] {
    filter { program("bar"); };
    parser {
        channel {
            rewrite { set("bar" value("BARVALUE")); };
        };
    };
};
"""


@pytest.mark.parametrize(
    "input_message, template, expected_value", test_parameters_first_match,
    ids=list(map(str, range(len(test_parameters_first_match)))),
)
def test_app_parser_first_match_without_overlaps_only_traverses_the_first_app(config, syslog_ng, input_message, template, expected_value):
    config.add_preamble(preamble_foobar_apps)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template + '\n'))
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log().strip() == expected_value


@pytest.mark.parametrize(
    "input_message, template, expected_value", test_parameters_all_matches,
    ids=list(map(str, range(len(test_parameters_all_matches)))),
)
def test_app_parser_allow_overlaps_causes_all_apps_to_be_traversed(config, syslog_ng, input_message, template, expected_value):
    config.add_preamble(preamble_foobar_apps)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify(input_message))
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog", allow_overlaps="yes")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template + '\n'))
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert file_destination.read_log()[:-1] == expected_value


def test_app_parser_dont_match_causes_the_message_to_be_dropped(config, syslog_ng):
    config.add_preamble(preamble_foobar_apps)
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("almafa message"))
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("foobar"))
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert "processed" not in file_destination.get_stats()


def test_app_parser_auto_parse_disabled_causes_the_message_to_be_dropped(config, syslog_ng):
    config.add_preamble(preamble_foobar_apps)
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("foo foomessage"))
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog", auto_parse="no")
    file_destination = config.create_file_destination(file_name="output.log")
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert "processed" not in file_destination.get_stats()


def test_app_parser_auto_parse_disabled_plus_overlaps_causes_the_message_to_be_accepted(config, syslog_ng):
    config.add_preamble(preamble_foobar_apps)
    config.update_global_options(stats_level=1)
    generator_source = config.create_example_msg_generator_source(num=1, template=config.stringify("foo foomessage"))
    syslog_parser = config.create_syslog_parser(flags="syslog-protocol")
    app_parser = config.create_app_parser(topic="syslog", auto_parse="no", allow_overlaps="yes")
    file_destination = config.create_file_destination(file_name="output.log")
    config.create_logpath(statements=[generator_source, syslog_parser, app_parser, file_destination])

    syslog_ng.start(config)
    assert "processed" not in file_destination.get_stats()
