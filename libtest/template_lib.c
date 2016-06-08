/*
 * Copyright (c) 2012-2015 Balabit
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

#include "testutils.h"
#include "template_lib.h"
#include "msg_parse_lib.h"

#include <string.h>

void
init_template_tests(void)
{
  init_and_load_syslogformat_module();
}

void
deinit_template_tests(void)
{
  deinit_syslogformat_module();
  if (!testutils_deinit())
    exit(1);
}

LogMessage *
message_from_list(va_list ap)
{
  char *key, *value;
  LogMessage *msg = create_empty_message();

  if (!msg)
    return NULL;

  key = va_arg(ap, char *);
  while (key)
    {
      value = va_arg(ap, char *);
      if (!value)
        return msg;

      log_msg_set_value_by_name(msg, key, value, -1);
      key = va_arg(ap, char *);
    }

  return msg;
}

LogMessage *
create_empty_message(void)
{
  LogMessage *msg;
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  GSockAddr *saddr;

  saddr = g_sockaddr_inet_new("10.11.12.13", 1010);
  msg = log_msg_new(msg_str, strlen(msg_str), saddr, &parse_options);
  g_sockaddr_unref(saddr);
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);
  log_msg_set_tag_by_name(msg, "alma");
  log_msg_set_tag_by_name(msg, "korte");
  log_msg_clear_tag_by_name(msg, "narancs");
  log_msg_set_tag_by_name(msg, "citrom");
  msg->rcptid = 555;
  msg->host_id = 0xcafebabe;

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(1139684315);

  return msg;
}

LogMessage *
create_sample_message(void)
{
  LogMessage *msg = create_empty_message();

  log_msg_set_value_by_name(msg, "APP.VALUE", "value", -1);
  log_msg_set_value_by_name(msg, "APP.STRIP1", "     value", -1);
  log_msg_set_value_by_name(msg, "APP.STRIP2", "value     ", -1);
  log_msg_set_value_by_name(msg, "APP.STRIP3", "     value     ", -1);
  log_msg_set_value_by_name(msg, "APP.STRIP4", "value", -1);
  log_msg_set_value_by_name(msg, "APP.STRIP5", "", -1);
  log_msg_set_value_by_name(msg, "APP.QVALUE", "\"value\"", -1);
  log_msg_set_value_by_name(msg, ".unix.uid", "1000", -1);
  log_msg_set_value_by_name(msg, ".unix.gid", "1000", -1);
  log_msg_set_value_by_name(msg, ".unix.cmd", "command", -1);
  log_msg_set_value_by_name(msg, ".json.foo", "bar", -1);
  log_msg_set_value_by_name(msg, ".json.sub.value1", "subvalue1", -1);
  log_msg_set_value_by_name(msg, ".json.sub.value2", "subvalue2", -1);
  log_msg_set_value_by_name(msg, "escaping", "binary stuff follows \"\xad árvíztűrőtükörfúrógép", -1);
  log_msg_set_value_by_name(msg, "escaping2", "\xc3", -1);
  log_msg_set_value_by_name(msg, "null", "binary\0stuff", 12);

  return msg;
}

LogTemplate *
compile_template(const gchar *template, gboolean escaping)
{
  LogTemplate *templ = log_template_new(configuration, NULL);
  gboolean success;
  GError *error = NULL;

  log_template_set_escape(templ, escaping);
  success = log_template_compile(templ, template, &error);
  expect_true(success, "template expected to compile cleanly,"
              " but it didn't, template=%s, error=%s",
              template, error ? error->message : "(none)");
  g_clear_error(&error);

  return templ;
}

void
assert_template_format(const gchar *template, const gchar *expected)
{
  assert_template_format_with_escaping(template, FALSE, expected);
}

void
assert_template_format_msg(const gchar *template, const gchar *expected,
                            LogMessage *msg)
{
  assert_template_format_with_escaping_msg(template, FALSE, expected, msg);
}


void
assert_template_format_with_escaping_msg(const gchar *template, gboolean escaping,
                                     const gchar *expected, LogMessage *msg)
{
  LogTemplate *templ = compile_template(template, escaping);
  if (!templ)
    return;

  GString *res = g_string_sized_new(128);
  const gchar *context_id = "test-context-id";

  log_template_format(templ, msg, NULL, LTZ_LOCAL, 999, context_id, res);
  expect_nstring(res->str, res->len, expected, strlen(expected),
                 "template test failed, template=%s", template);
  log_template_unref(templ);
  g_string_free(res, TRUE);
}

void
assert_template_format_with_escaping(const gchar *template, gboolean escaping,
                                     const gchar *expected)
{
  LogMessage *msg = create_sample_message();

  assert_template_format_with_escaping_msg(template, escaping, expected, msg);

  log_msg_unref(msg);
}

void
assert_template_format_with_context(const gchar *template, const gchar *expected)
{
  LogMessage *msg;
  LogMessage *context[2];

  msg = create_sample_message();
  context[0] = context[1] = msg;

  assert_template_format_with_context_msgs(template, expected, context, 2);

  log_msg_unref(msg);
}

void
assert_template_format_with_context_msgs(const gchar *template, const gchar *expected,
                                         LogMessage **msgs, gint num_messages)
{
  LogTemplate *templ = compile_template(template, FALSE);
  if (!templ)
    return;

  GString *res = g_string_sized_new(128);
  const gchar *context_id = "test-context-id";

  log_template_format_with_context(templ, msgs, num_messages, NULL, LTZ_LOCAL, 999, context_id, res);
  expect_nstring(res->str, res->len, expected, strlen(expected), "context template test failed, template=%s", template);
  log_template_unref(templ);
  g_string_free(res, TRUE);
}

void
assert_template_failure(const gchar *template, const gchar *expected_error)
{
  LogTemplate *templ = log_template_new(configuration, NULL);
  GError *error = NULL;

  expect_false(log_template_compile(templ, template, &error),
               "compilation failure expected to template,"
               " but success was returned, template=%s, expected_error=%s\n",
               template, expected_error);
  expect_true(strstr(error->message, expected_error) != NULL,
              "FAIL: compilation error doesn't match, error=%s, expected_error=%s\n",
              error->message, expected_error);
  g_clear_error(&error);
  log_template_unref(templ);
}
