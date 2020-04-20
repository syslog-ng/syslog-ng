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
#include "json-parser.h"
#include "apphook.h"
#include "msg_parse_lib.h"
#include <criterion/criterion.h>

static LogMessage *
parse_json_into_log_message_no_check(const gchar *json, LogParser *json_parser)
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
parse_json_into_log_message(const gchar *json, LogParser *json_parser)
{
  LogMessage *msg;

  msg = parse_json_into_log_message_no_check(json, json_parser);
  cr_assert_not_null(msg, "expected json-parser success and it returned failure, json=%s", json);
  return msg;
}

static void
assert_json_parser_fails(const gchar *json, LogParser *json_parser)
{
  LogMessage *msg;

  msg = parse_json_into_log_message_no_check(json, json_parser);
  cr_assert_null(msg, "expected json-parser failure and it returned success, json=%s", json);
}

void setup(void)
{
  app_startup();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(json_parser,  .init = setup, .fini = teardown);

Test(json_parser, test_json_parser_parses_well_formed_json_and_puts_results_in_message)
{
  LogMessage *msg;
  LogParser *json_parser = json_parser_new(NULL);

  msg = parse_json_into_log_message("{'foo': 'bar'}", json_parser);
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_adds_prefix_to_name_value_pairs_when_instructed)
{
  LogMessage *msg;
  LogParser *json_parser = json_parser_new(NULL);

  json_parser_set_prefix(json_parser, ".prefix.");
  msg = parse_json_into_log_message("{'foo': 'bar'}", json_parser);
  assert_log_message_value(msg, log_msg_get_value_handle(".prefix.foo"), "bar");
  log_msg_unref(msg);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_skips_marker_when_set_in_the_input)
{
  LogMessage *msg;
  LogParser *json_parser = json_parser_new(NULL);

  json_parser_set_marker(json_parser, "@cee:");
  msg = parse_json_into_log_message("@cee: {'foo': 'bar'}", json_parser);
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_fails_when_marker_is_not_present)
{
  LogParser *json_parser = json_parser_new(NULL);
  json_parser_set_marker(json_parser, "@cee:");
  assert_json_parser_fails("@cxx: {'foo': 'bar'}", json_parser);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_fails_for_invalid_json)
{
  LogParser *json_parser = json_parser_new(NULL);
  assert_json_parser_fails("not-valid-json", json_parser);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_validate_type_representation)
{
  LogMessage *msg;
  LogParser *json_parser = json_parser_new(NULL);

  json_parser_set_prefix(json_parser, ".prefix.");
  msg = parse_json_into_log_message("{'int': 123, 'booltrue': true, 'boolfalse': false, 'double': 1.23, 'object': {'member1': 'foo', 'member2': 'bar'}, 'array': [1, 2, 3], 'null': null}",
                                    json_parser);
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
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_fails_for_non_object_top_element)
{
  LogParser *json_parser = json_parser_new(NULL);
  assert_json_parser_fails("[1, 2, 3]", json_parser);
  assert_json_parser_fails("", json_parser);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_extracts_subobjects_if_extract_prefix_is_specified)
{
  LogMessage *msg;
  LogParser *json_parser = json_parser_new(NULL);

  json_parser_set_extract_prefix(json_parser, "[0]");
  msg = parse_json_into_log_message("[{'foo':'bar'}, {'bar':'foo'}]", json_parser);
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
  log_pipe_unref(&json_parser->super);
}

Test(json_parser, test_json_parser_works_with_templates)
{
  LogMessage *msg;
  LogTemplate *template;

  template = log_template_new(NULL, NULL);
  log_template_compile(template, "{'foo':'bar'}", NULL);

  LogParser *json_parser = json_parser_new(NULL);
  log_parser_set_template(json_parser, template);
  msg = parse_json_into_log_message("invalid-syntax-because-json-is-coming-from-the-template", json_parser);
  assert_log_message_value(msg, log_msg_get_value_handle("foo"), "bar");
  log_msg_unref(msg);
  log_pipe_unref(&json_parser->super);
}
