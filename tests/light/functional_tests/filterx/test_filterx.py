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
import json

import pytest

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
            "values.true_string" => string("boolean:true"),
            "values.false_string" => string("boolean:false"),
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
        filterx {{ {filterx_expr} \n}};
        destination(dest_true);
    }} else {{
        destination(dest_false);
    }};
}};
"""
    config.set_raw_config(preamble)
    return (file_true, file_false)

# TODO: take care of _evaluate_statement methods result, to fix this test
# def test_filterx_falsey_value_assign(config, syslog_ng):
#     (file_true, file_false) = create_config(
#         config, """
#                     $myvar = 0;
#                     $MSG = $myvar; """,
#     )
#     syslog_ng.start(config)

#     assert file_true.get_stats()["processed"] == 1
#     assert "processed" not in file_false.get_stats()
#     assert file_true.read_log() == "0\n"


def test_otel_logrecord_int32_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.dropped_attributes_count = ${values.int};
                                            $MSG = $olr.dropped_attributes_count; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "5\n"


def test_otel_logrecord_string_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.severity_text = "${values.str}";
                                            $MSG = $olr.severity_text; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "string\n"


def test_otel_logrecord_bytes_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.trace_id = ${values.bytes};
                                            $MSG = $olr.trace_id; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "binary whatever\n"


def test_otel_logrecord_datetime_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.time_unix_nano = ${values.datetime};
                                            $MSG = $olr.time_unix_nano; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "1701353998.123000+00:00\n"


def test_otel_logrecord_body_string_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.str};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "string\n"


def test_otel_logrecord_body_bool_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.bool};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "true\n"


def test_otel_logrecord_body_int_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.int};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "5\n"


def test_otel_logrecord_body_double_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.double};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "32.5\n"


def test_otel_logrecord_body_datetime_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.datetime};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    # there is no implicit conversion back to datetime because anyvalue field
    # does not distinguish int_value and datetime.
    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "1701353998123000\n"


# TODO: figure out null type's proper behaviour
# def test_otel_logrecord_body_null_setter_getter(config, syslog_ng):
#     (file_true, file_false) = create_config(
#         config, """
#                                             $olr = otel_logrecord();
#                                             $olr.body = ${values.null};
#                                             $MSG = $olr.body; """,
#     )
#     syslog_ng.start(config)

#     assert file_true.get_stats()["processed"] == 1
#     assert "processed" not in file_false.get_stats()
#     assert file_true.read_log() == "\n"


def test_otel_logrecord_body_bytes_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.bytes};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "binary whatever\n"


def test_otel_logrecord_body_protobuf_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.protobuf};
                                            $MSG = $olr.body; """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "this is not a valid protobuf!!\n"


def test_otel_logrecord_body_json_setter_getter(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
                                            $olr = otel_logrecord();
                                            $olr.body = ${values.json};
                                            istype($olr.body, "otel_kvlist");
                                            $MSG = format_json($olr.body); """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == '{"emb_key1":"emb_key1 value","emb_key2":"emb_key2 value"}\n'


def test_json_to_otel(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, r"""
                                            js = json({"foo": 42});
                                            js_arr = json_array([1, 2]);

                                            otel_kvl = otel_kvlist();

                                            otel_kvl.js = js;
                                            istype(otel_kvl.js, "otel_kvlist");
                                            otel_kvl.js += {"bar": 1337};

                                            otel_kvl.js_arr = js_arr;
                                            istype(otel_kvl.js_arr, "otel_array");
                                            otel_kvl.js_arr += [3, 4];

                                            $MSG = format_json(otel_kvl);
                """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == '{"js":{"foo":42,"bar":1337},"js_arr":[1,2,3,4]}\n'


def test_otel_to_json(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, r"""
                                            otel_kvl = otel_kvlist({"foo": 42});
                                            otel_arr = otel_array([1, 2]);

                                            js = json();

                                            js.otel_kvl = otel_kvl;
                                            istype(js.otel_kvl, "json_object");
                                            js.otel_kvl += {"bar": 1337};

                                            js.otel_arr = otel_arr;
                                            istype(js.otel_arr, "json_array");
                                            js.otel_arr += [3, 4];

                                            $MSG = js;
                """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == '{"otel_kvl":{"foo":42,"bar":1337},"otel_arr":[1,2,3,4]}\n'


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
    assert file_true.read_log() == """{"true_string":"boolean:true","str":"string","null":null,"list":["foo","bar","baz"],"json":{"emb_key1": "emb_key1 value", "emb_key2": "emb_key2 value"},"int":5,"false_string":"boolean:false","double":32.5,"datetime":"1701350398.123000+01:00","bool":true}\n"""


def test_json_assignment_from_another_name_value_pair(config, syslog_ng):

    (file_true, file_false) = create_config(config, """ $MSG = ${values.json}; """)
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1": "emb_key1 value", "emb_key2": "emb_key2 value"}\n"""


def test_json_getattr_returns_the_embedded_json(config, syslog_ng):

    (file_true, file_false) = create_config(
        config, """
$envelope = "$(format-json --subkeys values.)";
$MSG = $envelope.json.emb_key1;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """emb_key1 value\n"""


def test_json_setattr_sets_the_embedded_json(config, syslog_ng):

    (file_true, file_false) = create_config(
        config, """
$envelope = "$(format-json --subkeys values.)";
$envelope.json.new_key = "this is a new key!";
$MSG = $envelope.json;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1":"emb_key1 value","emb_key2":"emb_key2 value","new_key":"this is a new key!"}\n"""


def test_json_assign_performs_a_deep_copy(config, syslog_ng):

    (file_true, file_false) = create_config(
        config, """
$envelope = "$(format-json --subkeys values.)";
$envelope.json.new_key = "this is a new key!";
$MSG = $envelope.json;
$envelope.json.another_key = "this is another new key which is not added to $MSG!";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"emb_key1":"emb_key1 value","emb_key2":"emb_key2 value","new_key":"this is a new key!"}\n"""


def test_json_simple_literal_assignment(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json({
    "foo": "foovalue",
    "bar": "barvalue",
    "baz": "bazvalue"
});
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"foo":"foovalue","bar":"barvalue","baz":"bazvalue"}\n"""


def test_json_recursive_literal_assignment(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json({
    "foo": "foovalue",
    "bar": "barvalue",
    "baz": "bazvalue",
    "recursive": {
        "foo": "foovalue",
        "bar": "barvalue",
        "baz": "bazvalue",
        "recursive": {
            "foo": "foovalue",
            "bar": "barvalue",
            "baz": "bazvalue",
        },
    },
});
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"foo":"foovalue","bar":"barvalue","baz":"bazvalue","recursive":{"foo":"foovalue","bar":"barvalue","baz":"bazvalue","recursive":{"foo":"foovalue","bar":"barvalue","baz":"bazvalue"}}}\n"""


def test_json_change_recursive_literal(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json({
    "foo": "foovalue",
    "bar": "barvalue",
    "baz": "bazvalue",
    "recursive": {
        "foo": "foovalue",
        "bar": "barvalue",
        "baz": "bazvalue",
        "recursive": {
            "foo": "foovalue",
            "bar": "barvalue",
            "baz": "bazvalue",
        },
    },
});

$MSG.recursive.recursive.foo = "changedfoovalue";
$MSG.recursive.recursive.newattr = "newattrvalue";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """\
{"foo":"foovalue","bar":"barvalue","baz":"bazvalue",\
"recursive":{"foo":"foovalue","bar":"barvalue","baz":"bazvalue",\
"recursive":{"foo":"changedfoovalue","bar":"barvalue","baz":"bazvalue","newattr":"newattrvalue"}}}\n"""


def test_list_literal_becomes_syslogng_list_as_string(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json_array(["foo", "bar", "baz"]);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """foo,bar,baz\n"""


def test_list_literal_becomes_json_list_as_a_part_of_json(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$list = json_array(["foo", "bar", "baz"]);
$MSG = json({
    "key": "value",
    "list": $list,
});
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"key":"value","list":["foo","bar","baz"]}\n"""


def test_list_is_cloned_upon_assignment(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$list = json_array(["foo", "bar", "baz"]);
$MSG = $list;
$list[0] = "changed foo";
$MSG[2] = "changed baz";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    # foo remains unchanged while baz is changed
    assert file_true.read_log() == """foo,bar,"changed baz"\n"""


def test_list_subscript_without_index_appends_an_element(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$list = json_array();
$list[] = "foo";
$list[] = "bar";
$list[] = "baz";
$MSG = $list;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    # foo remains unchanged while baz is changed
    assert file_true.read_log() == """foo,bar,baz\n"""


def test_literal_generator_assignment(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json();
$MSG.foo = {"answer": 42, "leet": 1337};
$MSG["bar"] = {"answer+1": 43, "leet+1": 1338};
$MSG.list = ["will be replaced"];
$MSG.list[0] = [1, 2, 3];
$MSG.list[] = [4, 5, 6];
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"foo":{"answer":42,"leet":1337},"bar":{"answer+1":43,"leet+1":1338},"list":[[1,2,3],[4,5,6]]}\n"""


def test_literal_generator_casted_assignment(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$MSG = json();
$MSG.foo = json({"answer": 42, "leet": 1337});
$MSG["bar"] = json({"answer+1": 43, "leet+1": 1338});
$MSG.list = json_array(["will be replaced"]);
$MSG.list[0] = json_array([1, 2, 3]);
$MSG.list[] = json_array([4, 5, 6]);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == """{"foo":{"answer":42,"leet":1337},"bar":{"answer+1":43,"leet+1":1338},"list":[[1,2,3],[4,5,6]]}\n"""


def test_function_call(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
$list = json_array();
$list[] = "foo";
$list[] = "bar";
$list[] = "baz";
$MSG = example_echo($list);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    # foo remains unchanged while baz is changed
    assert file_true.read_log() == """foo,bar,baz\n"""


def test_ternary_operator_true(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = true?${values.true_string}:${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:true\n"


def test_ternary_operator_false(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = false?${values.true_string}:${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:false\n"


def test_ternary_operator_expression_true(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = (0 === 0)?${values.true_string}:${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:true\n"


def test_ternary_operator_expression_false(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = (0 === 1)?${values.true_string}:${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:false\n"


def test_ternary_operator_inline_ternary_expression_true(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = (0 === 0)?("foo" eq "foo"? ${values.true_string} : "inner:false"):${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:true\n"


def test_ternary_operator_inline_ternary_expression_false(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = (0 === 0)?("foo" eq "bar"? ${values.true_string} : "inner:false"):${values.false_string};
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "inner:false\n"


def test_ternary_return_condition_expression_value_without_true_branch(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = ${values.true_string}?:${values.false_string};
""",
    )

    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "boolean:true\n"


def test_if_condition_without_else_branch_match(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (true) {
        $out = "matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "matched\n"


def test_if_condition_without_else_branch_nomatch(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (false) {
        $out = "matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "default\n"


def test_if_condition_no_matching_condition(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (false) {
        $out = "matched";
    } elif (false) {
        $out = "elif-matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "default\n"


def test_if_condition_matching_main_condition(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (true) {
        $out = "matched";
    } elif (true) {
        $out = "elif-matched";
    } else {
        $out = "else-matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "matched\n"


def test_if_condition_matching_elif_condition(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (false) {
        $out = "matched";
    } elif (true) {
        $out = "elif-matched";
    } else {
        $out = "else-matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "elif-matched\n"


def test_if_condition_matching_else_condition(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if (false) {
        $out = "matched";
    } elif (false) {
        $out = "elif-matched";
    } else {
        $out = "else-matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "else-matched\n"


def test_if_condition_matching_expression(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if ("foo" eq "foo") {
        $out = "matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "matched\n"


def test_if_condition_non_matching_expression(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $out = "default";
    if ("foo" eq "bar") {
        $out = "matched";
    };
    $MSG = $out;
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "default\n"


def test_isset_existing_value_not_in_scope_yet(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    isset($MSG);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()


def test_isset_existing_value_already_in_scope(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG;
    isset($MSG);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()


def test_isset_inexisting_value_not_in_scope_yet(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    not isset($almafa);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()


def test_isset_inexisting_value_already_in_scope(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $almafa = "x";
    unset($almafa);
    not isset($almafa);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()


def test_isset(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = json();
    $MSG.inner_key = "foo";

    isset(${values.int});
    not isset($almafa);
    isset($MSG["inner_key"]);
    not isset($MSG["almafa"]);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()


def test_unset_value_not_in_scope_yet(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    unset($MSG);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "\n"


def test_unset_value_already_in_scope(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG;
    unset($MSG);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "\n"


def test_unset_existing_key(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = json();
    $MSG["foo"] = "bar";
    unset($MSG["foo"]);
    not isset($MSG["foo"]);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{}\n"


def test_unset_inexisting_key(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = json();
    unset($MSG["foo"]);
    not isset($MSG["foo"]);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{}\n"


def test_setting_an_unset_key_will_contain_the_right_value(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = json();
    $MSG["foo"] = "first";
    unset($MSG["foo"]);
    not isset($MSG["foo"]);
    $MSG["foo"] = "second";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == '{"foo":"second"}\n'


def test_unset(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = json();
    $MSG["inner_key"] = "foo";
    $arr = json_array();
    $arr[] = "first";
    $arr[] = "second";

    unset(${values.int});
    unset($almafa);
    unset($MSG["inner_key"]);
    unset($MSG["almafa"]);
    unset($arr[0]);

    not isset(${values.int});
    not isset($almafa);
    not isset($MSG["inner_key"]);
    not isset($MSG["almafa"]);
    isset($arr[0]);
    not isset($arr[1]);
    unset($MSG);
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "\n"


def test_strptime_error_result(config, syslog_ng):
    _ = create_config(
        config, """
        $MSG = strptime("2024-04-10T08:09:10Z"); # wrong arg set
""",
    )
    with pytest.raises(Exception):
        syslog_ng.start(config)


def test_strptime_success_result(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
        $MSG = strptime("2024-04-10T08:09:10Z", "%Y-%m-%dT%H:%M:%S%z");
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "1712736550.000000+00:00\n"


def test_strptime_failure_result(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
        $MSG = string(strptime("2024-04-10T08:09:10Z", "no", "valid", "parse", "fmt"));
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "null\n"


def test_len(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, r"""
    $dict = json();
    $list = json_array();
    len($dict) == 0;
    len($list) == 0;

    $dict.foo = "bar";
    $list[] = "foo";
    len($dict) == 1;
    len($list) == 1;

    len(${values.str}) == 6;

    $MSG = "success";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "success\n"


def test_regexp_match(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, r"""
    ${values.str} =~ /string/;
    ${values.str} =~ /.*/;
    not (${values.str} =~ /foobar/);
    "\"" =~ /"/;
    $MSG = "success";
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "success\n"


def test_regexp_match_error_in_pattern(config, syslog_ng):
    _ = create_config(
        config, r"""
    "foo" =~ /(/;
""",
    )
    with pytest.raises(Exception):
        syslog_ng.start(config)


def test_regexp_search(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, r"""
    $MSG = json();
    $MSG.unnamed = regexp_search("foobarbaz", /(foo)(bar)(baz)/);
    $MSG.named = regexp_search("foobarbaz", /(?<first>foo)(?<second>bar)(?<third>baz)/);
    $MSG.mixed = regexp_search("foobarbaz", /(?<first>foo)(bar)(?<third>baz)/);
    $MSG.force_list = json_array(regexp_search("foobarbaz", /(?<first>foo)(bar)(?<third>baz)/));
    $MSG.force_dict = json(regexp_search("foobarbaz", /(foo)(bar)(baz)/));

    $MSG.no_match_unnamed = regexp_search("foobarbaz", /(almafa)/);
    if (len($MSG.no_match_unnamed) == 0) {
        $MSG.no_match_unnamed_handling = true;
    };

    $MSG.no_match_named = regexp_search("foobarbaz", /(?<first>almafa)/);
    if (len($MSG.no_match_named) == 0) {
        $MSG.no_match_named_handling = true;
    };
""",
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert json.loads(file_true.read_log()) == {
        "unnamed": ["foobarbaz", "foo", "bar", "baz"],
        "named": {"0": "foobarbaz", "first": "foo", "second": "bar", "third": "baz"},
        "mixed": {"0": "foobarbaz", "first": "foo", "2": "bar", "third": "baz"},
        "force_list": ["foobarbaz", "foo", "bar", "baz"],
        "force_dict": {"0": "foobarbaz", "1": "foo", "2": "bar", "3": "baz"},
        "no_match_unnamed": [],
        "no_match_unnamed_handling": True,
        "no_match_named": {},
        "no_match_named_handling": True,
    }


def test_regexp_search_error_in_pattern(config, syslog_ng):
    _ = create_config(
        config, r"""
    $MSG = json(regexp_search("foo", /(/));
""",
    )
    with pytest.raises(Exception):
        syslog_ng.start(config)


def test_parse_kv_default_option_set_is_skippable(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = parse_kv("foo=bar, thisisstray bar=baz");
    """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == '{"foo":"bar","bar":"baz"}\n'


def test_parse_kv_value_separator(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = parse_kv("foo@bar, bar@baz", value_separator="@");
    """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{\"foo\":\"bar\",\"bar\":\"baz\"}\n"


def test_parse_kv_value_separator_use_first_character(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = parse_kv("foo@bar, bar@baz", value_separator="@!$");
    """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{\"foo\":\"bar\",\"bar\":\"baz\"}\n"


def test_parse_kv_pair_separator(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = parse_kv("foo=bar#bar=baz", pair_separator="#");
    """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{\"foo\":\"bar\",\"bar\":\"baz\"}\n"


def test_parse_kv_stray_words_value_name(config, syslog_ng):
    (file_true, file_false) = create_config(
        config, """
    $MSG = parse_kv("foo=bar, thisisstray bar=baz", stray_words_key="stray_words");
    """,
    )
    syslog_ng.start(config)

    assert file_true.get_stats()["processed"] == 1
    assert "processed" not in file_false.get_stats()
    assert file_true.read_log() == "{\"foo\":\"bar\",\"bar\":\"baz\",\"stray_words\":\"thisisstray\"}\n"
