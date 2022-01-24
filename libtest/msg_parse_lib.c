/*
 * Copyright (c) 2012-2019 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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
#include "plugin.h"

#include <criterion/criterion.h>

void
init_parse_options_and_load_syslogformat(MsgFormatOptions *parse_options)
{
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(parse_options);
  msg_format_options_init(parse_options, configuration);
}

void
deinit_syslogformat_module(void)
{
  if (configuration)
    cfg_free(configuration);
  configuration = NULL;
}

void
assert_log_message_has_tag(LogMessage *log_message, const gchar *tag_name)
{
  cr_assert(log_msg_is_tag_by_name(log_message, tag_name), "Expected message to have '%s' tag", tag_name);
}

void
assert_log_message_doesnt_have_tag(LogMessage *log_message, const gchar *tag_name)
{
  cr_assert_not(log_msg_is_tag_by_name(log_message, tag_name), "Expected message not to have '%s' tag", tag_name);
}

void
assert_log_message_value_unset(LogMessage *self, NVHandle handle)
{
  gssize key_name_length;
  const gchar *key_name = log_msg_get_value_name(handle, &key_name_length);
  const gchar *value = log_msg_get_value_if_set(self, handle, &key_name_length);

  cr_assert(value == NULL, "Expected value for key %s to be unset but the actual value is %s", key_name, value);
}

void
assert_log_message_value_unset_by_name(LogMessage *self, const gchar *name)
{
  assert_log_message_value_unset(self, log_msg_get_value_handle(name));
}

void
assert_log_message_value_and_type(LogMessage *self, NVHandle handle,
                                  const gchar *expected_value, LogMessageValueType expected_type)
{
  gssize key_name_length;
  gssize value_length;
  LogMessageValueType actual_type;
  const gchar *key_name = log_msg_get_value_name(handle, &key_name_length);
  const gchar *actual_value_r = log_msg_get_value_with_type(self, handle, &value_length, &actual_type);
  gchar *actual_value = g_strndup(actual_value_r, value_length);

  if (expected_value)
    {
      cr_assert_str_eq(actual_value, expected_value, "Invalid value for key %s; actual: %s, expected: %s", key_name,
                       actual_value, expected_value);
    }
  else
    cr_assert_str_eq(actual_value, "", "No value is expected for key %s but its value is %s", key_name, actual_value);

  if (expected_type != LM_VT_NONE)
    cr_assert_eq(actual_type, expected_type,
                 "Invalid value type for key %s; actual: %d, expected %d", key_name, actual_type, expected_type);

  g_free(actual_value);
}

void
assert_log_message_value(LogMessage *self, NVHandle handle, const gchar *expected_value)
{
  assert_log_message_value_and_type(self, handle, expected_value, LM_VT_NONE);
}

void
assert_log_message_value_and_type_by_name(LogMessage *self, const gchar *name,
                                          const gchar *expected_value, LogMessageValueType expected_type)
{
  assert_log_message_value_and_type(self, log_msg_get_value_handle(name), expected_value, expected_type);
}

void
assert_log_message_match_value_and_type(LogMessage *self, gint index_,
                                        const gchar *expected_value, LogMessageValueType expected_type)
{
  gssize value_length;
  LogMessageValueType actual_type;
  const gchar *actual_value_r = log_msg_get_match_with_type(self, index_, &value_length, &actual_type);
  gchar *actual_value = g_strndup(actual_value_r, value_length);

  if (expected_value)
    {
      cr_assert_str_eq(actual_value, expected_value, "Invalid value for $%d; actual: %s, expected: %s", index_,
                       actual_value, expected_value);
    }
  else
    cr_assert_str_eq(actual_value, "", "No value is expected for $%d but its value is %s", index_, actual_value);

  if (expected_type != LM_VT_NONE)
    cr_assert_eq(actual_type, expected_type,
                 "Invalid value type for $%d; actual: %d, expected %d", index_, actual_type, expected_type);

  g_free(actual_value);
}

void
assert_log_message_match_value(LogMessage *self, gint index_, const gchar *expected_value)
{
  assert_log_message_match_value_and_type(self, index_, expected_value, LM_VT_NONE);
}

void
assert_log_message_value_by_name(LogMessage *self, const gchar *name, const gchar *expected_value)
{
  assert_log_message_value_and_type_by_name(self, name, expected_value, LM_VT_NONE);
}

void
assert_log_messages_saddr(LogMessage *log_message_a, LogMessage *log_message_b)
{
  gchar address_a[256], address_b[256];

  g_sockaddr_format(log_message_a->saddr, address_a, sizeof(address_a), GSA_FULL);
  g_sockaddr_format(log_message_b->saddr, address_b, sizeof(address_b), GSA_FULL);
  cr_assert_str_eq(address_a, address_b, "Non-matching socket address; a: %s, b: %s", address_a, address_b);
}

void
assert_structured_data_of_messages(LogMessage *log_message_a, LogMessage *log_message_b)
{
  GString *structured_data_string_a = g_string_sized_new(0);
  GString *structured_data_string_b = g_string_sized_new(0);

  log_msg_format_sdata(log_message_a, structured_data_string_a, 0);
  log_msg_format_sdata(log_message_b, structured_data_string_b, 0);
  cr_assert_str_eq(structured_data_string_a->str, structured_data_string_b->str,
                   "Structure data string are not the same; a: %s, b: %s",
                   structured_data_string_a->str, structured_data_string_b->str);

  g_string_free(structured_data_string_a, TRUE);
  g_string_free(structured_data_string_b, TRUE);
}

void
assert_log_message_values_equal(LogMessage *log_message_a, LogMessage *log_message_b, NVHandle handle)
{
  gssize key_name_length;
  const gchar *key_name = log_msg_get_value_name(handle, &key_name_length);
  const gchar *value_a = log_msg_get_value(log_message_a, handle, NULL);
  const gchar *value_b = log_msg_get_value(log_message_b, handle, NULL);

  cr_assert_str_eq(value_a, value_b, "Non-matching value for key %s; a: %s, b: %s", key_name, value_a, value_b);
}

void
assert_log_messages_equal(LogMessage *log_message_a, LogMessage *log_message_b)
{
  cr_assert_eq(log_message_a->timestamps[LM_TS_STAMP].ut_sec, log_message_b->timestamps[LM_TS_STAMP].ut_sec,
               "Timestamps are not the same");
  cr_assert_eq(log_message_a->timestamps[LM_TS_STAMP].ut_usec, log_message_b->timestamps[LM_TS_STAMP].ut_usec,
               "Timestamps usec are not the same");
  cr_assert_eq(log_message_a->timestamps[LM_TS_STAMP].ut_gmtoff, log_message_b->timestamps[LM_TS_STAMP].ut_gmtoff,
               "Timestamp offset are not the same");

  cr_assert_eq(log_message_a->pri, log_message_b->pri, "Priorities are not the same");

  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_HOST);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_PROGRAM);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_MESSAGE);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_PID);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_MSGID);

  assert_structured_data_of_messages(log_message_a, log_message_b);
  assert_log_messages_saddr(log_message_a, log_message_b);
}
