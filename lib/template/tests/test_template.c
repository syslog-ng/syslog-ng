/*
 * Copyright (c) 2007-2018 Balabit
 * Copyright (c) 2007-2014 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "syslog-ng.h"
#include "libtest/cr_template.h"

#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "template/user-function.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "scratch-buffers.h"
#include "hostname.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

GCond thread_ping;
GMutex thread_lock;
gboolean thread_start;

static gpointer
format_template_thread(gpointer s)
{
  gpointer *args = (gpointer *) s;
  LogMessage *msg = args[0];
  LogTemplate *templ = args[1];
  const gchar *expected = args[2];
  GString *result;
  gint i;

  scratch_buffers_allocator_init();


  g_mutex_lock(&thread_lock);
  while (!thread_start)
    g_cond_wait(&thread_ping, &thread_lock);
  g_mutex_unlock(&thread_lock);

  result = g_string_sized_new(0);
  LogTemplateEvalOptions options = {NULL, LTZ_SEND, 5555, NULL};
  for (i = 0; i < 10000; i++)
    {
      log_template_format(templ, msg, &options, result);
      cr_assert_str_eq(result->str, expected, "multi-threaded formatting yielded invalid result (iteration: %d)", i);
      scratch_buffers_explicit_gc();
    }
  g_string_free(result, TRUE);
  scratch_buffers_allocator_deinit();
  return NULL;
}

static void
assert_template_format_multi_thread(const gchar *template, const gchar *expected)
{
  LogTemplate *templ;
  LogMessage *msg;
  gpointer args[3];
  GThread *threads[16];
  gint i;

  msg = create_sample_message();
  templ = compile_template(template, FALSE);
  args[0] = msg;
  args[1] = templ;
  args[2] = (gpointer) expected;

  thread_start = FALSE;
  g_cond_init(&thread_ping);
  g_mutex_init(&thread_lock);
  args[1] = templ;
  for (i = 0; i < 16; i++)
    {
      threads[i] = g_thread_new(NULL, format_template_thread, args);
    }

  thread_start = TRUE;
  g_mutex_lock(&thread_lock);
  g_cond_broadcast(&thread_ping);
  g_mutex_unlock(&thread_lock);
  for (i = 0; i < 16; i++)
    {
      g_thread_join(threads[i]);
    }
  g_cond_clear(&thread_ping);
  g_mutex_clear(&thread_lock);
  log_template_unref(templ);
  log_msg_unref(msg);
}


void
setup(void)
{
  app_startup();

  init_template_tests();
  cfg_load_module(configuration, "basicfuncs");
  configuration->template_options.frac_digits = 3;
  configuration->template_options.time_zone_info[LTZ_LOCAL] = time_zone_info_new(NULL);


  setenv("TZ", "MET-1METDST", TRUE);
  tzset();
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_template_tests();
  app_shutdown();
}

TestSuite(template, .init = setup, .fini = teardown);


Test(template, test_macros)
{
  /* pri 3, fac 19 == local3 */

  assert_template_format("$FACILITY", "local3");
  assert_template_format("$FACILITY_NUM", "19");

  assert_template_format("$SEVERITY", "err");
  assert_template_format("$SEVERITY_NUM", "3");

  assert_template_format("$PRIORITY", "err");
  assert_template_format("$LEVEL", "err");
  assert_template_format("$LEVEL_NUM", "3");

  assert_template_format("$TAG", "9b");
  assert_template_format("$TAGS", "alma,korte,citrom,\"tag,containing,comma\"");
  assert_template_format("$PRI", "155");
  assert_template_format("$DATE", "Feb 11 10:34:56.000");
  assert_template_format("$FULLDATE", "2006 Feb 11 10:34:56.000");
  assert_template_format("$ISODATE", "2006-02-11T10:34:56.000+01:00");
  assert_template_format("$STAMP", "Feb 11 10:34:56.000");
  assert_template_format("$YEAR", "2006");
  assert_template_format("$YEAR_DAY", "042");
  assert_template_format("$MONTH", "02");
  assert_template_format("$MONTH_WEEK", "1");
  assert_template_format("$MONTH_ABBREV", "Feb");
  assert_template_format("$MONTH_NAME", "February");
  assert_template_format("$DAY", "11");
  assert_template_format("$HOUR", "10");
  assert_template_format("$MIN", "34");
  assert_template_format("$SEC", "56");
  assert_template_format("$WEEKDAY", "Sat");
  assert_template_format("$WEEK_DAY", "7");
  assert_template_format("$WEEK_DAY_NAME", "Saturday");
  assert_template_format("$WEEK_DAY_ABBREV", "Sat");
  assert_template_format("$WEEK", "06");
  assert_template_format("$UNIXTIME", "1139650496.000");
  assert_template_format("$TZOFFSET", "+01:00");
  assert_template_format("$TZ", "+01:00");
  assert_template_format("$R_DATE", "Feb 11 19:58:35.639");
  assert_template_format("$R_FULLDATE", "2006 Feb 11 19:58:35.639");
  assert_template_format("$R_ISODATE", "2006-02-11T19:58:35.639+01:00");
  assert_template_format("$R_STAMP", "Feb 11 19:58:35.639");
  assert_template_format("$R_YEAR", "2006");
  assert_template_format("$R_YEAR_DAY", "042");
  assert_template_format("$R_MONTH", "02");
  assert_template_format("$R_MONTH_WEEK", "1");
  assert_template_format("$R_MONTH_ABBREV", "Feb");
  assert_template_format("$R_MONTH_NAME", "February");
  assert_template_format("$R_DAY", "11");
  assert_template_format("$R_HOUR", "19");
  assert_template_format("$R_MIN", "58");
  assert_template_format("$R_SEC", "35");
  assert_template_format("$R_WEEKDAY", "Sat");
  assert_template_format("$R_WEEK_DAY", "7");
  assert_template_format("$R_WEEK_DAY_NAME", "Saturday");
  assert_template_format("$R_WEEK_DAY_ABBREV", "Sat");
  assert_template_format("$R_WEEK", "06");
  assert_template_format("$R_UNIXTIME", "1139684315.639");
  assert_template_format("$R_TZOFFSET", "+01:00");
  assert_template_format("$R_TZ", "+01:00");
  assert_template_format("$S_DATE", "Feb 11 10:34:56.000");
  assert_template_format("$S_FULLDATE", "2006 Feb 11 10:34:56.000");
  assert_template_format("$S_ISODATE", "2006-02-11T10:34:56.000+01:00");
  assert_template_format("$S_STAMP", "Feb 11 10:34:56.000");
  assert_template_format("$S_YEAR", "2006");
  assert_template_format("$S_YEAR_DAY", "042");
  assert_template_format("$S_MONTH", "02");
  assert_template_format("$S_MONTH_WEEK", "1");
  assert_template_format("$S_MONTH_ABBREV", "Feb");
  assert_template_format("$S_MONTH_NAME", "February");
  assert_template_format("$S_DAY", "11");
  assert_template_format("$S_HOUR", "10");
  assert_template_format("$S_MIN", "34");
  assert_template_format("$S_SEC", "56");
  assert_template_format("$S_WEEKDAY", "Sat");
  assert_template_format("$S_WEEK_DAY", "7");
  assert_template_format("$S_WEEK_DAY_NAME", "Saturday");
  assert_template_format("$S_WEEK_DAY_ABBREV", "Sat");
  assert_template_format("$S_WEEK", "06");
  assert_template_format("$S_UNIXTIME", "1139650496.000");
  assert_template_format("$S_TZOFFSET", "+01:00");
  assert_template_format("$S_TZ", "+01:00");
  assert_template_format("$HOST_FROM", "kismacska");
  assert_template_format("$FULLHOST_FROM", "kismacska");
  assert_template_format("$HOST", "bzorp");
  assert_template_format("$FULLHOST", "bzorp");
  assert_template_format("$PROGRAM", "syslog-ng");
  assert_template_format("$PID", "23323");
  assert_template_format("$MSGHDR", "syslog-ng[23323]:");
  assert_template_format("$MSG", "árvíztűrőtükörfúrógép");
  assert_template_format("$MESSAGE", "árvíztűrőtükörfúrógép");
  assert_template_format("$SOURCEIP", "10.11.12.13");
  assert_template_format("$RCPTID", "555");
  assert_template_format("$DESTIP", "127.0.0.5");
  assert_template_format("$DESTPORT", "6514");
  assert_template_format("$PROTO", "33");

  assert_template_format("$SEQNUM", "999");
  assert_template_format("$CONTEXT_ID", "test-context-id");
  assert_template_format("$UNIQID", "cafebabe@000000000000022b");
}

Test(template, test_loghost_macro)
{
  const gchar *fqdn = get_local_hostname_fqdn();
  const gchar *short_name = get_local_hostname_short();

  /* by default $LOGHOST is using fqdn as the template options we are using uses use_fqdn by default */
  cr_assert_not(configuration->template_options.use_fqdn);
  configuration->template_options.use_fqdn = TRUE;
  assert_template_format("$LOGHOST", fqdn);
  configuration->template_options.use_fqdn = FALSE;
  assert_template_format("$LOGHOST", short_name);
}

Test(template, test_nvpairs)
{
  assert_template_format("$PROGRAM/var/log/messages/$HOST/$HOST_FROM/$MONTH$DAY${QQQQQ}valami",
                         "syslog-ng/var/log/messages/bzorp/kismacska/0211valami");
  assert_template_format("${APP.VALUE}", "value");
  assert_template_format("${APP.VALUE:-ures}", "value");
  assert_template_format("${APP.VALUE99:-ures}", "ures");
  assert_template_format("${1}", "first-match");
  assert_template_format("$1", "first-match");
  assert_template_format("$$$1$$", "$first-match$");
}

Test(template, test_template_functions)
{
  /* template functions */
  assert_template_format("$(echo $HOST $PID)", "bzorp 23323");
  assert_template_format("$(echo\n$HOST\n$PID)", "bzorp 23323");
  assert_template_format("$(echo \"$(echo $HOST)\" $PID)", "bzorp 23323");
  assert_template_format("$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  assert_template_format("$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  assert_template_format("$(echo '\"$(echo $(echo $HOST))\"' $PID)", "\"bzorp\" 23323");
}

Test(template, test_message_refs)
{
  /* message refs */
  assert_template_format_with_context("$(echo ${HOST}@0 ${PID}@1)", "bzorp 23323");
  assert_template_format_with_context("$(echo $HOST $PID)@0", "bzorp 23323");
}

Test(template, test_syntax_errors)
{
  /* template syntax errors */
  assert_template_failure("${unbalanced_brace", "'}' is missing");
  assert_template_format("$unbalanced_brace}", "}");
  assert_template_format("$}", "$}");
  assert_template_failure("$(unbalanced_paren", "missing function name or imbalanced '('");
  assert_template_format("$unbalanced_paren)", ")");
}

Test(template, test_multi_thread)
{
  /* name-value pair */
  assert_template_format_multi_thread("alma $HOST bela", "alma bzorp bela");
  assert_template_format_multi_thread("kukac $DATE mukac", "kukac Feb 11 10:34:56.000 mukac");
  assert_template_format_multi_thread("dani $(echo $HOST $DATE $(echo huha)) balint",
                                      "dani bzorp Feb 11 10:34:56.000 huha balint");
}

Test(template, test_escaping)
{
  assert_template_format_with_escaping("${APP.QVALUE}", FALSE, "\"value\"");
  assert_template_format_with_escaping("${APP.QVALUE}", TRUE, "\\\"value\\\"");
  assert_template_format_with_escaping("$(if (\"${APP.VALUE}\" eq \"value\") \"${APP.QVALUE}\" \"${APP.QVALUE}\")",
                                       FALSE, "\"value\"");
  assert_template_format_with_escaping("$(if (\"${APP.VALUE}\" eq \"value\") \"${APP.QVALUE}\" \"${APP.QVALUE}\")",
                                       TRUE, "\\\"value\\\"");
}

Test(template, test_user_template_function)
{
  LogTemplate *template;

  template = compile_template("this is a user-defined template function $DATE", FALSE);
  user_template_function_register(configuration, "dummy", template);
  assert_template_format("$(dummy)", "this is a user-defined template function Feb 11 10:34:56.000");
  assert_template_failure("$(dummy arg)", "User defined template function $(dummy) cannot have arguments");
  log_template_unref(template);
}

Test(template, test_template_function_args)
{
  assert_template_format("$(echo foo bar)", "foo bar");
  assert_template_format("$(echo 'foobar' \"barfoo\")", "foobar barfoo");
  assert_template_format("$(echo foo '' bar)", "foo  bar");
  assert_template_format("$(echo foo '')", "foo ");

  assert_template_failure("$(echo 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 "
                          "17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 "
                          "33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 "
                          "49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65)",
                          "Too many arguments (65)");
  assert_template_format("$(echo 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 "
                         "17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 "
                         "33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 "
                         "49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64)",
                         "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 "
                         "17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 "
                         "33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 "
                         "49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64");
}

static void
assert_template_trivial_value(const gchar *template_code, LogMessage *msg, const gchar *expected_value)
{
  LogTemplate *template = compile_template(template_code, FALSE);

  cr_assert(log_template_is_trivial(template));

  const gchar *trivial_value = log_template_get_trivial_value(template, msg, NULL);
  cr_assert_str_eq(trivial_value, expected_value);

  GString *formatted_value = g_string_sized_new(64);
  log_template_format(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value);
  cr_assert_str_eq(trivial_value, formatted_value->str,
                   "Formatted and trivial value does not match: '%s' - '%s'", trivial_value, formatted_value->str);

  g_string_free(formatted_value, TRUE);
  log_template_unref(template);
}


Test(template, test_single_values_and_literal_strings_are_considered_trivial)
{
  LogMessage *msg = create_sample_message();

  assert_template_trivial_value("", msg, "");
  assert_template_trivial_value(" ", msg, " ");
  assert_template_trivial_value("literal", msg, "literal");
  assert_template_trivial_value("$1", msg, "first-match");
  assert_template_trivial_value("$MSG", msg, "árvíztűrőtükörfúrógép");
  assert_template_trivial_value("$HOST", msg, "bzorp");
  assert_template_trivial_value("${APP.VALUE}", msg, "value");

  LogTemplate *template = log_template_new(configuration, NULL);
  cr_assert_not(log_template_compile(template, "$1 $2 ${MSG invalid", NULL));
  cr_assert(log_template_is_trivial(template), "Invalid templates are trivial");
  cr_assert(g_str_has_prefix(log_template_get_trivial_value(template, NULL, NULL), "error in template"));
  log_template_unref(template);

  log_msg_unref(msg);
}

Test(template, test_non_trivial_templates)
{
  LogTemplate *template;

  template = compile_template("$1", TRUE);
  cr_assert_not(log_template_is_trivial(template), "Escaped template is not trivial");
  log_template_unref(template);

  template = compile_template("$1 $2", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Multi-element template is not trivial");
  log_template_unref(template);

  template = compile_template("$1 literal", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Multi-element template is not trivial");
  log_template_unref(template);

  template = compile_template("pre${1}", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Single-value template with preliminary text is not trivial");
  log_template_unref(template);

  template = compile_template("${MSG}@3", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Template referencing non-last context element is not trivial");
  log_template_unref(template);

  template = compile_template("$(echo test)", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Template functions are not trivial");
  log_template_unref(template);

  template = compile_template("$DATE", FALSE);
  cr_assert_not(log_template_is_trivial(template), "Hard macros are not trivial");
  log_template_unref(template);
}

static void
assert_template_literal_value(const gchar *template_code, const gchar *expected_value)
{
  LogTemplate *template = compile_template(template_code, FALSE);

  cr_assert(log_template_is_literal_string(template));

  const gchar *literal_val = log_template_get_literal_value(template, NULL);
  cr_assert_str_eq(literal_val, expected_value);

  GString *formatted_value = g_string_sized_new(64);
  LogMessage *msg = create_sample_message();
  log_template_format(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value);
  cr_assert_str_eq(literal_val, formatted_value->str,
                   "Formatted and literal value does not match: '%s' - '%s'", literal_val, formatted_value->str);

  log_msg_unref(msg);
  g_string_free(formatted_value, TRUE);
  log_template_unref(template);
}

Test(template, test_literal_string_templates)
{
  assert_template_literal_value("", "");
  assert_template_literal_value(" ", " ");
  assert_template_literal_value("literal string", "literal string");
  assert_template_literal_value("$$not a macro", "$not a macro");

  LogTemplate *template = compile_template("a b c d $MSG", FALSE);
  cr_assert_not(log_template_is_literal_string(template));
  log_template_unref(template);
}

Test(template, test_compile_literal_string)
{
  LogTemplate *template = log_template_new(configuration, NULL);
  log_template_compile_literal_string(template, "test literal");

  cr_assert(log_template_is_literal_string(template));
  cr_assert(log_template_is_trivial(template));

  cr_assert_str_eq(log_template_get_literal_value(template, NULL), "test literal");

  log_template_unref(template);
}
