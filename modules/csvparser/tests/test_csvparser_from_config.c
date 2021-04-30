/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa <viktor.tusa@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "config_parse_lib.h"
#include "cr_template.h"

#include "scratch-buffers.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg-grammar.h"

void
cr_assert_msg_field_equals(LogMessage *msg, const gchar *field_name, const gchar *expected_value,
                           gssize expected_value_len)
{
  const gboolean result = assert_msg_field_equals_non_fatal(msg, field_name, expected_value, expected_value_len, NULL);
  cr_assert(result, "Message field assert");
}


LogMessage *
create_message_with_fields(const char *field_name, ...)
{
  va_list args;
  const char *arg;
  LogMessage *msg = log_msg_new_empty();

  msg->timestamps[LM_TS_STAMP].ut_sec = 365 * 24 * 3600;
  arg = field_name;
  va_start(args, field_name);
  while (arg != NULL)
    {
      NVHandle handle = log_msg_get_value_handle(arg);
      arg = va_arg(args, char *);
      log_msg_set_value(msg, handle, arg, -1);
      arg = va_arg(args, char *);
    }
  va_end(args);
  return msg;
}

LogMessage *
create_message_with_field(const char *field_name, const char *field_value)
{
  return create_message_with_fields(field_name, field_value, NULL);
}

LogParser *
create_parser_rule(const char *raw_parser_rule)
{
  char raw_config[1024];

  /* NOTE: the reference in the log statement is there to initialize the parser */
  snprintf(raw_config, sizeof(raw_config), "parser p_test{ %s }; log{ parser(p_test); };", raw_parser_rule);
  cr_assert(parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL), "Parsing the given configuration failed");
  cr_assert(cfg_init(configuration), "Config initialization failed");

  LogExprNode *expr_node = cfg_tree_get_object(&configuration->tree, ENC_PARSER, "p_test");
  return (LogParser *)expr_node->children->object;
}

void
invoke_parser_rule(LogParser *pipe_, LogMessage *msg)
{
  LogPathOptions po = LOG_PATH_OPTIONS_INIT;
  log_pipe_queue((LogPipe *) pipe_, log_msg_ref(msg), &po);
};

LogMessage *msg;

Test(parser, condition_success)
{
  LogParser *test_parser = create_parser_rule("csv-parser(columns('a', 'b', 'c'));");

  invoke_parser_rule(test_parser, msg);
  cr_assert_msg_field_equals(msg, "a", "foo", -1);
  cr_assert_msg_field_equals(msg, "b", "bar", -1);
  cr_assert_msg_field_equals(msg, "c", "baz", -1);
}

void
setup(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  start_grabbing_messages();
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "csvparser");
  msg = create_message_with_field("MSG", "foo bar baz");
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  log_msg_unref(msg);
  cfg_free(configuration);
  stop_grabbing_messages();
  app_shutdown();
}

TestSuite(parser, .init = setup, .fini = teardown);
