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
from src.common.blocking import wait_until_true
from src.syslog_ng_config.renderer import render_statement


def create_config(config, filterx_expr, msg="foobar"):
    file_true = config.create_file_destination(file_name="dest-true.log", template="'$MSG\n'")
    file_false = config.create_file_destination(file_name="dest-false.log", template="'$MSG\n'")

    preamble = f"""
@version: {config.get_version()}

options {{ stats(level(1)); }};

source genmsg {{
    example-msg-generator(
        num(1)
        template("{msg}")
        values(
            "values.str" => string("string"),
            "values.bool" => boolean(true),
            "values.int" => int(5),
            "values.double" => double(32.5),
            "values.datetime" => datetime("1701350398.123000+01:00"),
            "values.list" => list("foo,bar,baz"),
            "values.null" => null(""),
            "values.bytes" => bytes("binary whatever"),
            "values.protobuf" => protobuf("this is not a valid protobuf!!"),
            "values.json" => json('{{"emb_key1": "emb_key1 value", "emb_key2": "emb_key2 value"}}'),
        )
    );
}};

destination dest_true {{
    {render_statement(file_true)};
}};

destination dest_false {{
    {render_statement(file_false)};
}};

log {{
    source(genmsg);
    if {{
        filterx {{ {filterx_expr} }};
        destination(dest_true);
    }} else {{
        destination(dest_false);
    }};
}};
"""
    config.set_raw_config(preamble)
    return (file_true, file_false)


def test_simple_true_condition(config, syslog_ng):
    (file_true, file_false) = create_config(config, """ true; """)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "foobar\n"

def test_simple_false_condition(config, syslog_ng):
    (file_true, file_false) = create_config(config, """ false; """)
    syslog_ng.start(config)

    assert "processed" not in file_true.get_stats()
    assert file_false.get_stats()["processed"] == 1
    assert file_false.read_log() == "foobar\n"

def test_simple_assignment(config, syslog_ng):

    (file_true, file_false) = create_config(config, """ $MSG = "rewritten message!"; """)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "rewritten message!\n"

def test_json_assignment_from_template(config, syslog_ng):

    (file_true, file_false) = create_config(config, """ $MSG = "$(format-json --subkeys values.)"; """)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"str":"string","null":null,"list":["foo","bar","baz"],"json":{"emb_key1": "emb_key1 value", "emb_key2": "emb_key2 value"},"int":5,"double":32.5,"datetime":"1701350398.123000+01:00","bool":true}\n"""

def test_json_assignment_from_another_name_value_pair(config, syslog_ng):

    (file_true, file_false) = create_config(config, """ $MSG = ${values.json}; """)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1": "emb_key1 value", "emb_key2": "emb_key2 value"}\n"""

def test_json_getattr_returns_the_embedded_json(config, syslog_ng):

    (file_true, file_false) = create_config(config, """
$envelope = "$(format-json --subkeys values.)";
$MSG = $envelope.json.emb_key1;
""")
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """emb_key1 value\n"""

def test_json_setattr_sets_the_embedded_json(config, syslog_ng):

    (file_true, file_false) = create_config(config, """
$envelope = "$(format-json --subkeys values.)";
$envelope.json.new_key = "this is a new key!";
$MSG = $envelope.json;
""")
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1":"emb_key1 value","emb_key2":"emb_key2 value","new_key":"this is a new key!"}\n"""

def test_json_assign_performs_a_deep_copy(config, syslog_ng):

    (file_true, file_false) = create_config(config, """
$envelope = "$(format-json --subkeys values.)";
$envelope.json.new_key = "this is a new key!";
$MSG = $envelope.json;
$envelope.json.another_key = "this is another new key which is not added to $MSG!";
""")
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1":"emb_key1 value","emb_key2":"emb_key2 value","new_key":"this is a new key!"}\n"""
