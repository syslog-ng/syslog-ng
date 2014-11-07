/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "msg_parse_lib.h"
#include "apphook.h"
#include "logpipe.h"
#include "rcptid.h"
#include "libtest/persist_lib.h"

#include <stdlib.h>

LogMessage *
construct_log_message(void)
{
  const gchar *raw_msg = "foo";
  LogMessage *msg;

  msg = log_msg_new(raw_msg, strlen(raw_msg), NULL, &parse_options);
  log_msg_set_value(msg, LM_V_HOST, raw_msg, -1);
  return msg;
}

LogMessage *
clone_cow_log_message(LogMessage *msg)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  return log_msg_clone_cow(msg, &path_options);
}

static void
test_log_message_can_be_created_and_freed(void)
{
  LogMessage *msg = construct_log_message();
  log_msg_unref(msg);
}

NVHandle nv_handle;
NVHandle sd_handle;
const gchar *tag_name = "tag";

LogMessage *
construct_log_message_with_all_bells_and_whistles(void)
{
  LogMessage *msg = construct_log_message();

  nv_handle = log_msg_get_value_handle("foo");
  sd_handle = log_msg_get_value_handle(".SDATA.foo.bar");

  log_msg_set_value(msg, nv_handle, "value", -1);
  log_msg_set_value(msg, sd_handle, "value", -1);
  msg->saddr = g_sockaddr_inet_new("1.2.3.4", 5050);
  log_msg_set_tag_by_name(msg, tag_name);
  return msg;
}

void
assert_log_msg_clear_clears_all_properties(LogMessage *msg)
{
  log_msg_clear(msg);

  assert_string("", log_msg_get_value(msg, nv_handle, NULL), "Message still contains value after log_msg_clear");
  assert_string("", log_msg_get_value(msg, sd_handle, NULL), "Message still contains sdata value after log_msg_clear");
  assert_true(msg->saddr == NULL, "Message still contains an saddr after log_msg_clear");
  assert_false(log_msg_is_tag_by_name(msg, tag_name), "Message still contains a valid tag after log_msg_clear");
}

static void
test_log_message_can_be_cleared(void)
{
  LogMessage *msg, *clone;

  msg = construct_log_message_with_all_bells_and_whistles();
  clone = clone_cow_log_message(msg);

  assert_log_msg_clear_clears_all_properties(clone);
  log_msg_unref(clone);

  assert_log_msg_clear_clears_all_properties(msg);
  log_msg_unref(msg);
}


PersistState *state;

static void
setup_rcptid_test(void)
{
  state = clean_and_create_persist_state_for_test("test_values.persist");
  rcptid_init(state, TRUE);
}

static void
teardown_rcptid_test(void)
{
  commit_and_destroy_persist_state(state);
  rcptid_deinit();
}

static void
test_rcptid_is_automatically_assigned_to_a_newly_created_log_message(void)
{
  LogMessage *msg;
  
  setup_rcptid_test();
  msg = log_msg_new_empty();
  assert_guint64(msg->rcptid, 1, "rcptid is not automatically set");
  log_msg_unref(msg);
  teardown_rcptid_test();
}


void
test_log_message(void)
{
  MSG_TESTCASE(test_log_message_can_be_created_and_freed);
  MSG_TESTCASE(test_log_message_can_be_cleared);
  MSG_TESTCASE(test_rcptid_is_automatically_assigned_to_a_newly_created_log_message);
}

static LogMessage *
construct_merge_base_message(void)
{
  LogMessage *msg;

  msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "base", "basevalue", -1);
  log_msg_set_tag_by_name(msg, "basetag");
  return msg;
}

static LogMessage *
construct_merged_message(const gchar *name, const gchar *value)
{
  LogMessage *msg;

  msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, name, value, -1);
  log_msg_set_tag_by_name(msg, "mergedtag");
  return msg;
}

static void
test_log_message_merge_with_empty_context(void)
{
  LogMessage *msg, *msg_clone;
  LogMessage *context[] = {};

  msg = construct_log_message_with_all_bells_and_whistles();
  msg_clone = clone_cow_log_message(msg);
  log_msg_merge_context(msg, context, 0);
  log_msg_unref(msg);
  assert_log_messages_equal(msg, msg_clone);
}


static void
test_log_message_merge_unset_value(void)
{
  LogMessage *msg;
  GPtrArray *context = g_ptr_array_sized_new(0);

  msg = construct_merge_base_message();
  g_ptr_array_add(context, construct_merged_message("merged", "mergedvalue"));
  log_msg_merge_context(msg, (LogMessage **) context->pdata, context->len);

  assert_log_message_value_by_name(msg, "base", "basevalue");
  assert_log_message_value_by_name(msg, "merged", "mergedvalue");
  g_ptr_array_foreach(context, (GFunc) log_msg_unref, NULL);
  g_ptr_array_free(context, TRUE);
  log_msg_unref(msg);
}

static void
test_log_message_merge_doesnt_overwrite_already_set_values(void)
{
  LogMessage *msg;
  GPtrArray *context = g_ptr_array_sized_new(0);

  msg = construct_merge_base_message();
  g_ptr_array_add(context, construct_merged_message("base", "mergedvalue"));
  log_msg_merge_context(msg, (LogMessage **) context->pdata, context->len);

  assert_log_message_value_by_name(msg, "base", "basevalue");
  g_ptr_array_foreach(context, (GFunc) log_msg_unref, NULL);
  g_ptr_array_free(context, TRUE);
  log_msg_unref(msg);
}

static void
test_log_message_merge_merges_the_closest_value_in_the_context(void)
{
  LogMessage *msg;
  GPtrArray *context = g_ptr_array_sized_new(0);

  msg = construct_merge_base_message();
  g_ptr_array_add(context, construct_merged_message("merged", "mergedvalue1"));
  g_ptr_array_add(context, construct_merged_message("merged", "mergedvalue2"));
  log_msg_merge_context(msg, (LogMessage **) context->pdata, context->len);

  assert_log_message_value_by_name(msg, "merged", "mergedvalue2");
  g_ptr_array_foreach(context, (GFunc) log_msg_unref, NULL);
  g_ptr_array_free(context, TRUE);
  log_msg_unref(msg);
}

static void
test_log_message_merge_merges_from_all_messages_in_the_context(void)
{
  LogMessage *msg;
  GPtrArray *context = g_ptr_array_sized_new(0);

  msg = construct_merge_base_message();
  g_ptr_array_add(context, construct_merged_message("merged1", "mergedvalue1"));
  g_ptr_array_add(context, construct_merged_message("merged2", "mergedvalue2"));
  g_ptr_array_add(context, construct_merged_message("merged3", "mergedvalue3"));
  log_msg_merge_context(msg, (LogMessage **) context->pdata, context->len);

  assert_log_message_value_by_name(msg, "merged1", "mergedvalue1");
  assert_log_message_value_by_name(msg, "merged2", "mergedvalue2");
  assert_log_message_value_by_name(msg, "merged3", "mergedvalue3");
  g_ptr_array_foreach(context, (GFunc) log_msg_unref, NULL);
  g_ptr_array_free(context, TRUE);
  log_msg_unref(msg);
}

static void
test_log_message_merge_leaves_base_tags_intact(void)
{
  LogMessage *msg;
  GPtrArray *context = g_ptr_array_sized_new(0);

  msg = construct_merge_base_message();
  g_ptr_array_add(context, construct_merged_message("merged1", "mergedvalue1"));
  log_msg_merge_context(msg, (LogMessage **) context->pdata, context->len);

  assert_log_message_has_tag(msg, "basetag");
  assert_log_message_doesnt_have_tag(msg, "mergedtag");
  g_ptr_array_foreach(context, (GFunc) log_msg_unref, NULL);
  g_ptr_array_free(context, TRUE);
  log_msg_unref(msg);
}

static void
test_log_message_merge(void)
{
  MSG_TESTCASE(test_log_message_merge_with_empty_context);
  MSG_TESTCASE(test_log_message_merge_unset_value);
  MSG_TESTCASE(test_log_message_merge_doesnt_overwrite_already_set_values);
  MSG_TESTCASE(test_log_message_merge_merges_the_closest_value_in_the_context);
  MSG_TESTCASE(test_log_message_merge_merges_from_all_messages_in_the_context);
  MSG_TESTCASE(test_log_message_merge_leaves_base_tags_intact);
}

void
test_log_msg_get_value_with_time_related_macro(void)
{
  LogMessage *msg;
  gssize value_len;
  NVHandle handle;
  const char *date_value;

  msg = log_msg_new_empty();
  msg->timestamps[LM_TS_STAMP].tv_sec = 1389783444;

  handle = log_msg_get_value_handle("ISODATE");
  date_value = log_msg_get_value(msg, handle, &value_len);
  assert_string(date_value, "2014-01-15T10:57:23-00:00", "ISODATE macro value does not match!");

  log_msg_unref(msg);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  init_and_load_syslogformat_module();

  test_log_message();
  test_log_message_merge();
  test_log_msg_get_value_with_time_related_macro();

  app_shutdown();
  return 0;
}
