/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2014 Balazs Scheidler <bazsi@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include <criterion/criterion.h>
#include "apphook.h"
#include "msg_parse_lib.h"
#include "dot-notation.h"

#include <json.h>

static struct json_object *

compile_json(const gchar *json)
{
  struct json_tokener *tok;
  struct json_object *jso;

  tok = json_tokener_new();
  jso = json_tokener_parse_ex(tok, json, strlen(json));
  cr_assert(tok->err == json_tokener_success, "expected to parse input json, but couldn't, json=%s", json);
  json_tokener_free(tok);
  return jso;
}

static void
assert_json_equals(struct json_object *a, struct json_object *b, const gchar *subscript)
{
  const gchar *a_str, *b_str;

  a_str = json_object_to_json_string(a);
  b_str = json_object_to_json_string(b);
  cr_assert_str_eq(a_str, b_str, "extraction didn't return the expected subscript of the object: %s", subscript);
}

static void
assert_dot_notation_eval_equals(const gchar *input_json, const gchar *subscript, const gchar *expected_json)
{
  struct json_object *input, *expected, *sub;

  input = compile_json(input_json);
  expected = compile_json(expected_json);

  sub = json_extract(input, subscript);
  assert_json_equals(sub, expected, subscript);
  json_object_put(expected);
  json_object_put(input);
}

static void
assert_dot_notation_eval_fails(const gchar *input_json, const gchar *subscript)
{
  struct json_object *input, *sub;

  input = compile_json(input_json);
  sub = json_extract(input, subscript);
  cr_assert_null(sub, "extracted JSON is not NULL as expected, json=%s, subscript=%s", input_json, subscript);
  json_object_put(input);
}

void setup(void)
{
  app_startup();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(json, .init = setup, .fini = teardown);

Test(json, test_dot_notation_eval_empty_subscript_returns_the_object)
{
  assert_dot_notation_eval_equals("{'foo': 'bar'}", "", "{'foo': 'bar'}");
}

Test(json, test_dot_notation_eval_fails_with_an_identifier_that_doesnt_start_with_a_letter)
{
  assert_dot_notation_eval_fails("{}", "123");
}

Test(json, test_dot_notation_eval_fails_with_an_identifier_that_contains_a_non_alnum_character)
{
  assert_dot_notation_eval_fails("{}", "foo123_?");
}

Test(json, test_dot_notation_eval_fails_with_an_incorrect_array_reference)
{
  assert_dot_notation_eval_fails("[]", "foo[1]bar");
  assert_dot_notation_eval_fails("[]", "foo[zbc]");
  assert_dot_notation_eval_fails("{'foo': ['1', '2', '3']}", "foo.[0]");
}

Test(json, test_dot_notation_eval_succeeds_with_an_identifier_that_contains_all_possible_character_classes)
{
  assert_dot_notation_eval_equals("{'fOo123_': 'bar'}", "fOo123_", "'bar'");
}

Test(json, test_dot_notation_eval_object_subscript_extracts_the_child_object)
{
  assert_dot_notation_eval_equals("{'foo': 'bar'}", "foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo': {'foo': 'bar'}}", "foo", "{'foo': 'bar'}");
  assert_dot_notation_eval_equals("{'foo': {'foo': 'bar'}}", "foo.foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo': {'foo': {'foo': 'bar'}}}", "foo.foo.foo", "'bar'");
}

Test(json, test_dot_notation_eval_object_succeeds_with_odd_or_invalid_js_identifier)
{
  assert_dot_notation_eval_equals("{'@foo': 'bar'}", "@foo", "'bar'");
  assert_dot_notation_eval_equals("{'_foo': 'bar'}", "_foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo+4': 'bar'}", "foo+4", "'bar'");
  assert_dot_notation_eval_equals("{'foo,bar': 'bar'}", "foo,bar", "'bar'");
  assert_dot_notation_eval_equals("{'foo bar': 'bar'}", "foo bar", "'bar'");
  assert_dot_notation_eval_equals("{'foo-bar': 'bar'}", "foo-bar", "'bar'");
  assert_dot_notation_eval_equals("{'1': 'bar'}", "1", "'bar'");
}

Test(json, test_dot_notation_eval_object_member_from_non_object_fails)
{
  assert_dot_notation_eval_fails("[1, 2, 3]", "foo");
}

Test(json, test_dot_notation_eval_array_item_from_non_array_fails)
{
  assert_dot_notation_eval_fails("{'foo': 'bar'}", "[0]");
}

Test(json, test_dot_notation_eval_array_item_by_index)
{
  assert_dot_notation_eval_equals("['foo', 'bar', 'baz']", "[0]", "'foo'");
  assert_dot_notation_eval_equals("['foo', 'bar', 'baz']", "[1]", "'bar'");
  assert_dot_notation_eval_equals("{'foo': 'bar', 'baz': ['1', '2', '3']}", "baz[2]", "'3'");
}

Test(json, test_dot_notation_eval_multi_array_item_by_multiple_indexes)
{
  assert_dot_notation_eval_equals("{'foo': 'bar', 'baz':"
                                  "["
                                  "[ '1', '2', '3' ],"
                                  "[ '4', '5', '6' ],"
                                  "[ '7', '8', '9' ]"
                                  "]}", "baz[0][2]", "'3'");
}

Test(json, test_dot_notation_eval_fails_with_out_of_bounds_index)
{
  assert_dot_notation_eval_fails("['foo', 'bar', 'baz']", "[3]");
  assert_dot_notation_eval_fails("['foo', 'bar', 'baz']", "[-1]");
}
