/*
 * Copyright (c) 2014 Balabit
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
#include "testutils.h"
#include "apphook.h"
#include "msg_parse_lib.h"
#include "dot-notation.h"

#include <json.h>

#define dot_notation_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
    }                                                           \
  while (0)

#define dot_notation_testcase_end()                           \
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)


#define DOT_NOTATION_TESTCASE(x, ...) \
  do {                                                          \
      dot_notation_testcase_begin(#x, #__VA_ARGS__);      \
      x(__VA_ARGS__);                                           \
      dot_notation_testcase_end();                              \
  } while(0)


static struct json_object *
compile_json(const gchar *json)
{
  struct json_tokener *tok;
  struct json_object *jso;

  tok = json_tokener_new();
  jso = json_tokener_parse_ex(tok, json, strlen(json));
  assert_true(tok->err == json_tokener_success, "expected to parse input json, but couldn't, json=%s", json);
  json_tokener_free(tok);
  return jso;
}

static void
assert_json_equals(struct json_object *a, struct json_object *b, const gchar *subscript)
{
  const gchar *a_str, *b_str;

  a_str = json_object_to_json_string(a);
  b_str = json_object_to_json_string(b);
  assert_string(a_str, b_str, "extraction didn't return the expected subscript of the object: %s", subscript);
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
  assert_null(sub, "extracted JSON is not NULL as expected, json=%s, subscript=%s", input_json, subscript);
  json_object_put(input);
}

static void
test_dot_notation_eval_empty_subscript_returns_the_object(void)
{
  assert_dot_notation_eval_equals("{'foo': 'bar'}", "", "{'foo': 'bar'}");
}

static void
test_dot_notation_eval_fails_with_an_identifier_that_doesnt_start_with_a_letter(void)
{
  assert_dot_notation_eval_fails("{}", "123");
}

static void
test_dot_notation_eval_fails_with_an_identifier_that_contains_a_non_alnum_character(void)
{
  assert_dot_notation_eval_fails("{}", "foo123_?");
}

static void
test_dot_notation_eval_fails_with_an_incorrect_array_reference(void)
{
  assert_dot_notation_eval_fails("[]", "foo[1]bar");
  assert_dot_notation_eval_fails("[]", "foo[zbc]");
  assert_dot_notation_eval_fails("{'foo': ['1', '2', '3']}", "foo.[0]");
}

static void
test_dot_notation_eval_succeeds_with_an_identifier_that_contains_all_possible_character_classes(void)
{
  assert_dot_notation_eval_equals("{'fOo123_': 'bar'}", "fOo123_", "'bar'");
}

static void
test_dot_notation_eval_object_subscript_extracts_the_child_object(void)
{
  assert_dot_notation_eval_equals("{'foo': 'bar'}", "foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo': {'foo': 'bar'}}", "foo", "{'foo': 'bar'}");
  assert_dot_notation_eval_equals("{'foo': {'foo': 'bar'}}", "foo.foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo': {'foo': {'foo': 'bar'}}}", "foo.foo.foo", "'bar'");
}

static void
test_dot_notation_eval_object_succeeds_with_odd_or_invalid_js_identifier(void)
{
  assert_dot_notation_eval_equals("{'@foo': 'bar'}", "@foo", "'bar'");
  assert_dot_notation_eval_equals("{'_foo': 'bar'}", "_foo", "'bar'");
  assert_dot_notation_eval_equals("{'foo+4': 'bar'}", "foo+4", "'bar'");
  assert_dot_notation_eval_equals("{'foo,bar': 'bar'}", "foo,bar", "'bar'");
  assert_dot_notation_eval_equals("{'foo bar': 'bar'}", "foo bar", "'bar'");
  assert_dot_notation_eval_equals("{'foo-bar': 'bar'}", "foo-bar", "'bar'");
  assert_dot_notation_eval_equals("{'1': 'bar'}", "1", "'bar'");
}

static void
test_dot_notation_eval_object_member_from_non_object_fails(void)
{
  assert_dot_notation_eval_fails("[1, 2, 3]", "foo");
}

static void
test_dot_notation_eval_array_item_from_non_array_fails(void)
{
  assert_dot_notation_eval_fails("{'foo': 'bar'}", "[0]");
}

static void
test_dot_notation_eval_array_item_by_index(void)
{
  assert_dot_notation_eval_equals("['foo', 'bar', 'baz']", "[0]", "'foo'");
  assert_dot_notation_eval_equals("['foo', 'bar', 'baz']", "[1]", "'bar'");
  assert_dot_notation_eval_equals("{'foo': 'bar', 'baz': ['1', '2', '3']}", "baz[2]", "'3'");
}


static void
test_dot_notation_eval_multi_array_item_by_multiple_indexes(void)
{
  assert_dot_notation_eval_equals("{'foo': 'bar', 'baz':"
                                  "["
                                  "[ '1', '2', '3' ],"
                                  "[ '4', '5', '6' ],"
                                  "[ '7', '8', '9' ]"
                                  "]}", "baz[0][2]", "'3'");
}

static void
test_dot_notation_eval_fails_with_out_of_bounds_index(void)
{
  assert_dot_notation_eval_fails("['foo', 'bar', 'baz']", "[3]");
  assert_dot_notation_eval_fails("['foo', 'bar', 'baz']", "[-1]");
}

static void
test_dot_notation_eval(void)
{
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_empty_subscript_returns_the_object);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_object_subscript_extracts_the_child_object);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_object_succeeds_with_odd_or_invalid_js_identifier);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_fails_with_an_identifier_that_doesnt_start_with_a_letter);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_fails_with_an_identifier_that_contains_a_non_alnum_character);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_fails_with_an_incorrect_array_reference);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_succeeds_with_an_identifier_that_contains_all_possible_character_classes);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_object_member_from_non_object_fails);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_array_item_from_non_array_fails);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_array_item_by_index);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_multi_array_item_by_multiple_indexes);
  DOT_NOTATION_TESTCASE(test_dot_notation_eval_fails_with_out_of_bounds_index);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  test_dot_notation_eval();
  app_shutdown();
  return 0;
}
