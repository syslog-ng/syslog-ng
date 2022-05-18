/*
 * Copyright (c) 2012-2018 Balabit
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

#include "cr_template.h"
#include "stopwatch.h"
#include "msg_parse_lib.h"

#include "timeutils/misc.h"
#include "logmsg/logmsg.h"
#include "gsockaddr.h"
#include "cfg.h"

#include <string.h>
#include <criterion/criterion.h>


static MsgFormatOptions parse_options;

void
init_template_tests(void)
{
  init_parse_options_and_load_syslogformat(&parse_options);
}

void
deinit_template_tests(void)
{
  deinit_syslogformat_module();
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
  const gchar *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  msg = msg_format_parse(&parse_options, (const guchar *) msg_str, strlen(msg_str));
  log_msg_set_saddr_ref(msg, g_sockaddr_inet_new("10.11.12.13", 1010));
  log_msg_set_daddr_ref(msg, g_sockaddr_inet_new("127.0.0.5", 6514));
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);
  log_msg_set_tag_by_name(msg, "alma");
  log_msg_set_tag_by_name(msg, "korte");
  log_msg_clear_tag_by_name(msg, "narancs");
  log_msg_set_tag_by_name(msg, "citrom");
  log_msg_set_tag_by_name(msg, "tag,containing,comma");
  msg->rcptid = 555;
  msg->host_id = 0xcafebabe;
  msg->proto = 33;

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].ut_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].ut_usec = 639000;
  msg->timestamps[LM_TS_RECVD].ut_gmtoff = get_local_timezone_ofs(1139684315);

  return msg;
}

LogMessage *
create_sample_message(void)
{
  LogMessage *msg = create_empty_message();

  log_msg_set_value_by_name(msg, "APP.VALUE", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE2", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE3", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE4", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE5", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE6", "value", -1);
  log_msg_set_value_by_name(msg, "APP.VALUE7", "value", -1);

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
  log_msg_set_value_by_name(msg, "comma_value", "value,with,a,comma", -1);
  log_msg_set_value_by_name(msg, "empty_value", "", -1);
  log_msg_set_value_by_name(msg, "template_name", "dummy", -1);
  log_msg_set_value_by_name_with_type(msg, "number1", "123", -1, LM_VT_INT64);
  log_msg_set_value_by_name_with_type(msg, "number2", "456", -1, LM_VT_INT64);

  return msg;
}

LogTemplate *
compile_template_with_escaping(const gchar *template, gboolean escaping)
{
  LogTemplate *templ = log_template_new(configuration, NULL);
  gboolean success;
  GError *error = NULL;

  log_template_set_escape(templ, escaping);
  success = log_template_compile(templ, template, &error);
  cr_assert(success, "template expected to compile cleanly,"
            " but it didn't, template=%s, error=%s",
            template, error ? error->message : "(none)");
  g_clear_error(&error);

  return templ;
}

LogTemplate *
compile_template(const gchar *template)
{
  return compile_template_with_escaping(template, FALSE);
}

LogTemplate *
compile_escaped_template(const gchar *template)
{
  return compile_template_with_escaping(template, TRUE);
}

void
assert_template_format_with_escaping_and_context_msgs(const gchar *template, gboolean escaping,
                                                      const gchar *expected, gssize expected_len,
                                                      LogMessageValueType expected_type,
                                                      LogMessage **msgs, gint num_messages)
{
  LogTemplate *templ = compile_template_with_escaping(template, escaping);
  const gchar *prefix = "somevoodooprefix/";
  LogMessageValueType type;
  gint prefix_len = strlen(prefix);
  if (!templ)
    return;

  GString *res = g_string_new(prefix);
  const gchar *context_id = "test-context-id";

  LogTemplateEvalOptions options = {NULL, LTZ_LOCAL, 999, context_id, LM_VT_STRING};
  log_template_append_format_value_and_type_with_context(templ, msgs, num_messages, &options, res, &type);
  cr_assert(strncmp(res->str, prefix, prefix_len) == 0,
            "the prefix was overwritten by the template, template=%s, res=%s, expected_prefix=%s",
            template, res->str, prefix);

  expected_len = (expected_len >= 0 ? expected_len : strlen(expected));
  cr_assert_eq(res->len - prefix_len, expected_len,
               "context template test failed, expected length mismatch, template=%s, actual=%.*s, expected=%.*s",
               template, (gint) res->len - prefix_len, res->str + prefix_len, (gint) expected_len, expected);

  cr_assert_arr_eq(res->str + prefix_len, expected, expected_len,
                   "context template test failed, template=%s, actual=%.*s, expected=%.*s",
                   template, (gint) res->len - prefix_len, res->str + prefix_len, (gint) expected_len, expected);
  if (expected_type != LM_VT_NONE)
    cr_assert_eq(type, expected_type,
                 "expected type does not match template=%s, type=%d, expected_type=%d",
                 template, type, expected_type);
  log_template_unref(templ);
  g_string_free(res, TRUE);
}

void
assert_template_format_with_context_msgs(const gchar *template, const gchar *expected, LogMessage **msgs,
                                         gint num_messages)
{
  assert_template_format_with_escaping_and_context_msgs(template, FALSE, expected, -1, LM_VT_NONE, msgs, num_messages);
}


void
assert_template_format_with_escaping_msg(const gchar *template, gboolean escaping,
                                         const gchar *expected,
                                         LogMessage *msg)
{
  assert_template_format_with_escaping_and_context_msgs(template, escaping, expected, -1, LM_VT_NONE, &msg, 1);
}

void
assert_template_format_with_escaping(const gchar *template, gboolean escaping, const gchar *expected)
{
  LogMessage *msg = create_sample_message();

  assert_template_format_with_escaping_msg(template, escaping, expected, msg);
  log_msg_unref(msg);
}

void
assert_template_format_msg(const gchar *template, const gchar *expected, LogMessage *msg)
{
  assert_template_format_with_escaping_msg(template, FALSE, expected, msg);
}

void
assert_template_format(const gchar *template, const gchar *expected)
{
  assert_template_format_with_escaping(template, FALSE, expected);
}

void
assert_template_format_value_and_type(const gchar *template, const gchar *expected, LogMessageValueType expected_type)
{
  LogMessage *msg = create_sample_message();

  assert_template_format_with_escaping_and_context_msgs(template, FALSE, expected, -1, expected_type, &msg, 1);
  log_msg_unref(msg);
}

void
assert_template_format_value_and_type_with_escaping(const gchar *template, gboolean escaping, const gchar *expected,
                                                    LogMessageValueType expected_type)
{
  LogMessage *msg = create_sample_message();

  assert_template_format_with_escaping_and_context_msgs(template, escaping, expected, -1, expected_type, &msg, 1);
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
assert_template_format_with_len(const gchar *template, const gchar *expected, gssize expected_len)
{
  LogMessage *msg = create_sample_message();

  assert_template_format_with_escaping_and_context_msgs(template, FALSE, expected, expected_len, LM_VT_NONE, &msg, 1);
  log_msg_unref(msg);
}

void
assert_template_failure(const gchar *template, const gchar *expected_error)
{
  LogTemplate *templ = log_template_new(configuration, NULL);
  GError *error = NULL;

  cr_assert_not(log_template_compile(templ, template, &error),
                "compilation failure expected to template,"
                " but success was returned, template=%s, expected_error=%s\n",
                template, expected_error);
  cr_assert(strstr(error ? error->message : "", expected_error) != NULL,
            "FAIL: compilation error doesn't match, error=%s, expected_error=%s\n",
            error->message, expected_error);
  g_clear_error(&error);
  log_template_unref(templ);
}

#define BENCHMARK_COUNT 100000

void
perftest_template(gchar *template)
{
  LogTemplate *templ;
  LogMessage *msg;
  GString *res = g_string_sized_new(1024);
  gint i;
  GError *error = NULL;

  templ = log_template_new(configuration, NULL);
  if (!log_template_compile(templ, template, &error))
    {
      cr_assert(FALSE, "template expected to compile cleanly,"
                " but it didn't, template=%s, error=%s",
                template, error ? error->message : "(none)");
      return;
    }
  msg = create_sample_message();

  start_stopwatch();
  for (i = 0; i < BENCHMARK_COUNT; i++)
    {
      log_template_format(templ, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, res);
    }
  stop_stopwatch_and_display_result(BENCHMARK_COUNT,
                                    "      %-90.*s",
                                    (int) strlen(template) - 1,
                                    template);

  log_template_unref(templ);
  g_string_free(res, TRUE);
  log_msg_unref(msg);
}
