/*
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

#include "apphook.h"
#include "plugin.h"
#include <criterion/criterion.h>
#include "config_parse_lib.h"
#include "cfg-grammar.h"
#include "rewrite/rewrite-expr.h"

void
expect_config_parse_failure(const char *raw_rewrite_rule)
{
  char raw_config[1024];

  configuration = cfg_new_snippet();
  snprintf(raw_config, sizeof(raw_config), "rewrite s_test{ %s };", raw_rewrite_rule);
  cr_assert_not(parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL), "Parsing the given configuration failed");
  cfg_free(configuration);
};

LogRewrite *
create_rewrite_rule(const char *raw_rewrite_rule)
{
  char raw_config[1024];

  configuration = cfg_new_snippet();
  snprintf(raw_config, sizeof(raw_config), "rewrite s_test{ %s }; log{ rewrite(s_test); };", raw_rewrite_rule);
  cr_assert(parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL), "Parsing the given configuration failed");
  cr_assert(cfg_init(configuration), "Config initialization failed");

  LogExprNode *expr_node = cfg_tree_get_object(&configuration->tree, ENC_REWRITE, "s_test");
  return (LogRewrite *)expr_node->children->object;
}

LogMessage *
create_message_with_fields(const char *field_name, ...)
{
  va_list args;
  const char *arg;
  LogMessage *msg = log_msg_new_empty();

  msg->timestamps[LM_TS_STAMP].tv_sec = 365 * 24 * 3600;
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

void
invoke_rewrite_rule(LogRewrite *pipe_, LogMessage *msg)
{
  LogPathOptions po = LOG_PATH_OPTIONS_INIT;
  log_pipe_queue((LogPipe *) pipe_, log_msg_ref(msg), &po);
};

void
rewrite_teardown(LogMessage *msg)
{
  log_msg_unref(msg);
  cfg_free(configuration);
}

void
cr_assert_msg_field_equals(LogMessage *msg, const gchar *field_name, const gchar *expected_value,
                           gssize expected_value_len)
{
  const gboolean result = assert_msg_field_equals_non_fatal(msg, field_name, expected_value, expected_value_len, "");
  cr_assert(result, "Message field assert");
}

Test(rewrite, condition_success)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"00100\", value(\"device_id\") condition(program(\"ARCGIS\")));");
  LogMessage *msg = create_message_with_field("PROGRAM", "ARCGIS");
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "device_id", "00100", -1);
  rewrite_teardown(msg);
}

Test(rewrite, reference_on_condition_cloned)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"00100\", value(\"device_id\") condition(program(\"ARCGIS\")));");
  LogPipe *cloned_rule = log_pipe_clone(&test_rewrite->super);
  cr_assert_eq(test_rewrite->condition->ref_cnt, 2, "Bad reference number of condition");
  log_pipe_unref(cloned_rule);
  cr_assert_eq(test_rewrite->condition->ref_cnt, 1, "Bad reference number of condition");
  cfg_free(configuration);
}

Test(rewrite, set_field_exist_and_set_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"value\" value(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "oldvalue");
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_not_exist_and_set_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"value\" value(\"field1\") );");
  LogMessage *msg = log_msg_new_empty();
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_set_template_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"$field2\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "field2","newvalue", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "newvalue", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_field_exist_and_substring_substituted)
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "asubstringb", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "asubstituteb", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_pcre_unused_subpattern)
{
  LogRewrite *test_rewrite =
    create_rewrite_rule("subst('(a|(z))(bc)', '.', value('field1') type(pcre) flags('store-matches'));");
  LogMessage *msg = create_message_with_fields("field1", "abc", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", ".", -1);
  cr_assert_msg_field_equals(msg, "0", "abc", -1);
  cr_assert_msg_field_equals(msg, "1", "a", -1);
  cr_assert_msg_field_equals(msg, "2", "", -1);
  cr_assert_msg_field_equals(msg, "3", "bc", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_field_exist_and_substring_substituted_with_template)
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"$field2\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "asubstringb", "field2","substitute", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "asubstituteb", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_field_exist_and_substring_substituted_only_once_without_global)
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "substring substring", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "substitute substring", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_field_exist_and_substring_substituted_every_occurrence_with_global)
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") flags(global) );");
  LogMessage *msg = create_message_with_fields("field1", "substring substring", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "substitute substitute", -1);
  rewrite_teardown(msg);
}

Test(rewrite, subst_field_exist_and_substring_substituted_when_regexp_matched)
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"[0-9]+\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "a123b", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "asubstituteb", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "oldvalue");
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_honors_time_zone)
{
  LogRewrite *test_rewrite = create_rewrite_rule("set('${ISODATE}' value('UTCDATE') time-zone('Asia/Tokyo'));");
  LogMessage *msg = create_message_with_fields("field1", "a123b", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "UTCDATE", "1971-01-01T09:00:00+09:00", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_multiple_fields_with_glob_pattern_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field.*\") );");
  LogMessage *msg = create_message_with_fields("field.name1", "oldvalue","field.name2", "oldvalue", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field.name1", "value", -1);
  cr_assert_msg_field_equals(msg, "field.name2", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_multiple_fields_with_glob_question_mark_pattern_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field?\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue","field2", "oldvalue", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  cr_assert_msg_field_equals(msg, "field2", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_multiple_fields_with_multiple_glob_pattern_literal_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\" \"field2\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue","field2", "oldvalue", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  cr_assert_msg_field_equals(msg, "field2", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_template_string)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"$field2\" values(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "field2", "value", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_template_string_with_old_value)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"$_ alma\" values(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "value");
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value alma", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_when_condition_doesnt_match)
{
  LogRewrite *test_rewrite =
    create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program1\") ) );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "PROGRAM", "program2", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "oldvalue", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_exist_and_group_set_when_condition_matches)
{
  LogRewrite *test_rewrite =
    create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program\") ) );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "PROGRAM", "program", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "value", -1);
  rewrite_teardown(msg);
}

Test(rewrite, set_field_cloned)
{
  LogRewrite *test_rewrite =
    create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program\") ) );");
  LogPipe *cloned_rule = log_pipe_clone(&test_rewrite->super);
  cr_assert(cloned_rule != NULL, "Can't cloned the rewrite");
  log_pipe_unref(cloned_rule);
  cfg_free(configuration);
}

Test(rewrite, set_field_invalid_template)
{
  expect_config_parse_failure("groupset(\"${alma\" values(\"field1\") );");
}

Test(rewrite, unset_field_disappears)
{
  LogRewrite *test_rewrite = create_rewrite_rule("unset(value('field1'));");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "PROGRAM", "foobar", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "", -1);
  cr_assert_msg_field_equals(msg, "PROGRAM", "foobar", -1);
  rewrite_teardown(msg);
}

Test(rewrite, groupunset_field_disappears)
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupunset(values('field?'));");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "field2", "oldvalue2", "PROGRAM", "foobar", NULL);
  invoke_rewrite_rule(test_rewrite, msg);
  cr_assert_msg_field_equals(msg, "field1", "", -1);
  cr_assert_msg_field_equals(msg, "field2", "", -1);
  cr_assert_msg_field_equals(msg, "PROGRAM", "foobar", -1);
  rewrite_teardown(msg);
}

void
setup(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  start_grabbing_messages();
}

void
teardown(void)
{
  stop_grabbing_messages();
  app_shutdown();
}

TestSuite(rewrite, .init = setup, .fini = teardown);

