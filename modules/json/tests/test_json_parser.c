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
#include "json-parser.h"
#include "apphook.h"
#include "msg_parse_lib.h"

#define json_parser_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      json_parser = json_parser_new(NULL);                      \
    }                                                           \
  while (0)

#define json_parser_testcase_end()                           \
  do                                                            \
    {                                                           \
      log_pipe_unref(&json_parser->super);                      \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define JSON_PARSER_TESTCASE(x, ...) \
  do {                                                          \
      json_parser_testcase_begin(#x, #__VA_ARGS__);  		\
      x(__VA_ARGS__);                                           \
      json_parser_testcase_end();                               \
  } while(0)

LogParser *json_parser;

static LogMessage *
parse_json_into_log_message_no_check(const gchar *json)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogParser *cloned_parser;

  cloned_parser = (LogParser *) log_pipe_clone(&json_parser->super);

  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, json, -1);
  if (!log_parser_process_message(cloned_parser, &msg, &path_options))
    {
      log_msg_unref(msg);
      log_pipe_unref(&cloned_parser->super);
      return NULL;
    }
  log_pipe_unref(&cloned_parser->super);
  return msg;
}

static LogMessage *
parse_json_into_log_message(const gchar *json)
{
  LogMessage *msg;

  msg = parse_json_into_log_message_no_check(json);
  assert_not_null(msg, "expected json-parser success and it returned failure, json=%s", json);
  return msg;
}

static void
assert_json_parser_fails(const gchar *json)
{
  LogMessage *msg;

  msg = parse_json_into_log_message_no_check(json);
  assert_null(msg, "expected json-parser failure and it returned success, json=%s", json);
}

static void
test_json_parser_parses_well_formed_json_and_puts_results_in_message(void)
{
  LogMessage *msg;
  
  msg = parse_json_into_log_message("{'foo': 'bar'}");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
}

static void
test_json_parser_adds_prefix_to_name_value_pairs_when_instructed(void)
{
  LogMessage *msg;

  json_parser_set_prefix(json_parser, ".prefix.");
  msg = parse_json_into_log_message("{'foo': 'bar'}");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.foo"), "bar");
  log_msg_unref(msg);
}

static void
test_json_parser_skips_marker_when_set_in_the_input(void)
{
  LogMessage *msg;

  json_parser_set_marker(json_parser, "@cee:");
  msg = parse_json_into_log_message("@cee: {'foo': 'bar'}");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
}

static void
test_json_parser_fails_when_marker_is_not_present(void)
{
  json_parser_set_marker(json_parser, "@cee:");
  assert_json_parser_fails("@cxx: {'foo': 'bar'}");
}

static void
test_json_parser_fails_for_invalid_json(void)
{
  assert_json_parser_fails("not-valid-json");
}

static void
test_json_parser_validate_type_representation(void)
{
  LogMessage *msg;

  json_parser_set_prefix(json_parser, ".prefix.");
  msg = parse_json_into_log_message("{'int': 123, 'booltrue': true, 'boolfalse': false, 'double': 1.23, 'object': {'member1': 'foo', 'member2': 'bar'}, 'array': [1, 2, 3], 'null': null}");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.int"), "123");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.booltrue"), "true");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.boolfalse"), "false");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.double"), "1.230000");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.object.member1"), "foo");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.object.member2"), "bar");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.array[0]"), "1");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.array[1]"), "2");
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.array[2]"), "3");
  log_msg_unref(msg);
}

static void
test_json_parser_fails_for_non_object_top_element(void)
{
  assert_json_parser_fails("[1, 2, 3]");
  assert_json_parser_fails("");
}

static void
test_json_parser_extracts_subobjects_if_extract_prefix_is_specified(void)
{
  LogMessage *msg;

  json_parser_set_extract_prefix(json_parser, "[0]");
  msg = parse_json_into_log_message("[{'foo':'bar'}, {'bar':'foo'}]");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
}

static void
test_json_parser_works_with_templates(void)
{
  LogMessage *msg;
  LogTemplate *template;

  template = log_template_new(NULL, NULL);
  log_template_compile(template, "{'foo':'bar'}", NULL);

  log_parser_set_template(json_parser, template);
  msg = parse_json_into_log_message("invalid-syntax-because-json-is-coming-from-the-template");
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
}

static void
test_json_parser(void)
{
  JSON_PARSER_TESTCASE(test_json_parser_parses_well_formed_json_and_puts_results_in_message);
  JSON_PARSER_TESTCASE(test_json_parser_adds_prefix_to_name_value_pairs_when_instructed);
  JSON_PARSER_TESTCASE(test_json_parser_skips_marker_when_set_in_the_input);
  JSON_PARSER_TESTCASE(test_json_parser_fails_when_marker_is_not_present);
  JSON_PARSER_TESTCASE(test_json_parser_fails_for_invalid_json);
  JSON_PARSER_TESTCASE(test_json_parser_validate_type_representation);
  JSON_PARSER_TESTCASE(test_json_parser_fails_for_non_object_top_element);
  JSON_PARSER_TESTCASE(test_json_parser_extracts_subobjects_if_extract_prefix_is_specified);
  JSON_PARSER_TESTCASE(test_json_parser_works_with_templates);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  test_json_parser();
  app_shutdown();
  return 0;
}
