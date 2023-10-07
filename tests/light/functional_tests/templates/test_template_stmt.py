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


def test_template_stmt_with_identifier_reference(config, syslog_ng):
    template = config.create_template("template with $(format-welf test.*)\n")
    config.add_template(template)

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=template.name)

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]


def test_template_with_non_string_values(config, syslog_ng):

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    rewrites = [
        config.create_rewrite_set("int(10)", value="values_int_hint"),
        config.create_rewrite_set("float(4.5)", value="values_float_hint"),
        config.create_rewrite_set("10", value="values_int_literal"),
        config.create_rewrite_set("4.5", value="values_float_literal")
    ]
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$(format-json values_*)\n"))

    config.create_logpath(statements=[generator_source, *rewrites, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["""{"values_int_literal":10,"values_int_hint":10,"values_float_literal":4.5,"values_float_hint":4.5}\n"""]


def test_template_stmt_with_indirect_invocation(config, syslog_ng):
    template = config.create_template("template with $(format-welf test.*)\n")
    config.add_template(template)

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2 template_fn => {}".format(template.name))
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$(template {} error resolving template)\n".format(template.name)))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]


def test_simple_template_stmt_with_identifier_reference(config, syslog_ng):
    template = config.create_template("template with $(format-welf test.*)\n", use_simple_statement=True)
    config.add_template(template)

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=template.name)

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]


def test_template_stmt_with_string_reference(config, syslog_ng):
    template = config.create_template("template with $(format-welf test.*)\n")
    config.add_template(template)

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify(template.name))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]


def test_inline_template(config, syslog_ng):
    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("template with $(format-welf test.*)\n"))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]


def test_template_function(config, syslog_ng):
    template = config.create_template_function("template with $(format-welf test.*)\n", name="test_template_fn")
    config.add_template(template)

    generator_source = config.create_example_msg_generator_source(num=1, values="test.key1 => value1 test.key2 => value2")
    file_destination = config.create_file_destination(file_name="output.log", template=config.stringify("$(test_template_fn)\n"))

    config.create_logpath(statements=[generator_source, file_destination])
    syslog_ng.start(config)
    log = file_destination.read_logs(1)
    assert log == ["template with test.key1=value1 test.key2=value2\n"]
