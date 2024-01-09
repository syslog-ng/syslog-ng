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

#include <iv.h>

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

  iv_init();
  scratch_buffers_allocator_init();


  g_mutex_lock(&thread_lock);
  while (!thread_start)
    g_cond_wait(&thread_ping, &thread_lock);
  g_mutex_unlock(&thread_lock);

  result = g_string_sized_new(0);
  LogTemplateEvalOptions options = {NULL, LTZ_SEND, 5555, NULL, LM_VT_STRING};
  for (i = 0; i < 10000; i++)
    {
      log_template_format(templ, msg, &options, result);
      cr_assert_str_eq(result->str, expected, "multi-threaded formatting yielded invalid result (iteration: %d)", i);
      scratch_buffers_explicit_gc();
    }
  g_string_free(result, TRUE);
  scratch_buffers_allocator_deinit();
  iv_deinit();
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
  templ = compile_template(template);
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


Test(template, test_macros_v3x)
{
  /* pri 3, fac 19 == local3 */

  /* v3.x behavior */
  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);

  assert_template_format_value_and_type("$FACILITY", "local3", LM_VT_STRING);
  assert_template_format_value_and_type("$FACILITY_NUM", "19", LM_VT_STRING);

  assert_template_format_value_and_type("$SEVERITY", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$SEVERITY_NUM", "3", LM_VT_STRING);

  assert_template_format_value_and_type("$PRIORITY", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$LEVEL", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$LEVEL_NUM", "3", LM_VT_STRING);

  assert_template_format_value_and_type("$TAG", "9b", LM_VT_STRING);
  assert_template_format_value_and_type("$TAGS", "alma,korte,citrom,\"tag,containing,comma\"", LM_VT_STRING);
  assert_template_format_value_and_type("$PRI", "155", LM_VT_STRING);
  assert_template_format_value_and_type("$DATE", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLDATE", "2006 Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$ISODATE", "2006-02-11T10:34:56.000+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$STAMP", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$HOUR", "10", LM_VT_STRING);
  assert_template_format_value_and_type("$MIN", "34", LM_VT_STRING);
  assert_template_format_value_and_type("$SEC", "56", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$UNIXTIME", "1139650496.000", LM_VT_STRING);
  assert_template_format_value_and_type("$TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_DATE", "Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_FULLDATE", "2006 Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_ISODATE", "2006-02-11T19:58:35.639+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_STAMP", "Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$R_YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$R_DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$R_HOUR", "19", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MIN", "58", LM_VT_STRING);
  assert_template_format_value_and_type("$R_SEC", "35", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$R_UNIXTIME", "1139684315.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_DATE", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_FULLDATE", "2006 Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_ISODATE", "2006-02-11T10:34:56.000+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_STAMP", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$S_YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$S_DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$S_HOUR", "10", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MIN", "34", LM_VT_STRING);
  assert_template_format_value_and_type("$S_SEC", "56", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$S_UNIXTIME", "1139650496.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$HOST_FROM", "kismacska", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLHOST_FROM", "kismacska", LM_VT_STRING);
  assert_template_format_value_and_type("$HOST", "bzorp", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLHOST", "bzorp", LM_VT_STRING);
  assert_template_format_value_and_type("$PROGRAM", "syslog-ng", LM_VT_STRING);
  assert_template_format_value_and_type("$PID", "23323", LM_VT_STRING);
  assert_template_format_value_and_type("$MSGHDR", "syslog-ng[23323]:", LM_VT_STRING);
  assert_template_format_value_and_type("$MSG", "árvíztűrőtükörfúrógép", LM_VT_STRING);
  assert_template_format_value_and_type("$MESSAGE", "árvíztűrőtükörfúrógép", LM_VT_STRING);
  assert_template_format_value_and_type("$SOURCEIP", "10.11.12.13", LM_VT_STRING);
  assert_template_format_value_and_type("$RCPTID", "555", LM_VT_STRING);
  assert_template_format_value_and_type("$DESTIP", "127.0.0.5", LM_VT_STRING);
  assert_template_format_value_and_type("$DESTPORT", "6514", LM_VT_STRING);
  assert_template_format_value_and_type("$PROTO", "33", LM_VT_STRING);
  assert_template_format_value_and_type("$IP_PROTO", "4", LM_VT_STRING);

  assert_template_format_value_and_type("$SEQNUM", "999", LM_VT_STRING);
  assert_template_format_value_and_type("$CONTEXT_ID", "test-context-id", LM_VT_STRING);
  assert_template_format_value_and_type("$UNIQID", "cafebabe@000000000000022b", LM_VT_STRING);
}

Test(template, test_macros_v40)
{
  /* v4.x */
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  assert_template_format_value_and_type("$FACILITY", "local3", LM_VT_STRING);
  assert_template_format_value_and_type("$FACILITY_NUM", "19", LM_VT_INTEGER);

  assert_template_format_value_and_type("$SEVERITY", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$SEVERITY_NUM", "3", LM_VT_INTEGER);

  assert_template_format_value_and_type("$PRIORITY", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$LEVEL", "err", LM_VT_STRING);
  assert_template_format_value_and_type("$LEVEL_NUM", "3", LM_VT_INTEGER);

  assert_template_format_value_and_type("$TAG", "9b", LM_VT_STRING);
  assert_template_format_value_and_type("$TAGS", "alma,korte,citrom,\"tag,containing,comma\"", LM_VT_LIST);
  assert_template_format_value_and_type("$PRI", "155", LM_VT_STRING);
  assert_template_format_value_and_type("$DATE", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLDATE", "2006 Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$ISODATE", "2006-02-11T10:34:56.000+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$STAMP", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$HOUR", "10", LM_VT_STRING);
  assert_template_format_value_and_type("$MIN", "34", LM_VT_STRING);
  assert_template_format_value_and_type("$SEC", "56", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$UNIXTIME", "1139650496.000", LM_VT_DATETIME);
  assert_template_format_value_and_type("$TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_DATE", "Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_FULLDATE", "2006 Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_ISODATE", "2006-02-11T19:58:35.639+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_STAMP", "Feb 11 19:58:35.639", LM_VT_STRING);
  assert_template_format_value_and_type("$R_YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$R_YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$R_DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$R_HOUR", "19", LM_VT_STRING);
  assert_template_format_value_and_type("$R_MIN", "58", LM_VT_STRING);
  assert_template_format_value_and_type("$R_SEC", "35", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$R_WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$R_UNIXTIME", "1139684315.639", LM_VT_DATETIME);
  assert_template_format_value_and_type("$R_TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$R_TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_DATE", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_FULLDATE", "2006 Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_ISODATE", "2006-02-11T10:34:56.000+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_STAMP", "Feb 11 10:34:56.000", LM_VT_STRING);
  assert_template_format_value_and_type("$S_YEAR", "2006", LM_VT_STRING);
  assert_template_format_value_and_type("$S_YEAR_DAY", "042", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH", "02", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_WEEK", "1", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_ABBREV", "Feb", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MONTH_NAME", "February", LM_VT_STRING);
  assert_template_format_value_and_type("$S_DAY", "11", LM_VT_STRING);
  assert_template_format_value_and_type("$S_HOUR", "10", LM_VT_STRING);
  assert_template_format_value_and_type("$S_MIN", "34", LM_VT_STRING);
  assert_template_format_value_and_type("$S_SEC", "56", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEKDAY", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY", "7", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY_NAME", "Saturday", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK_DAY_ABBREV", "Sat", LM_VT_STRING);
  assert_template_format_value_and_type("$S_WEEK", "06", LM_VT_STRING);
  assert_template_format_value_and_type("$S_UNIXTIME", "1139650496.000", LM_VT_DATETIME);
  assert_template_format_value_and_type("$S_TZOFFSET", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$S_TZ", "+01:00", LM_VT_STRING);
  assert_template_format_value_and_type("$HOST_FROM", "kismacska", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLHOST_FROM", "kismacska", LM_VT_STRING);
  assert_template_format_value_and_type("$HOST", "bzorp", LM_VT_STRING);
  assert_template_format_value_and_type("$FULLHOST", "bzorp", LM_VT_STRING);
  assert_template_format_value_and_type("$PROGRAM", "syslog-ng", LM_VT_STRING);
  assert_template_format_value_and_type("$PID", "23323", LM_VT_STRING);
  assert_template_format_value_and_type("$MSGHDR", "syslog-ng[23323]:", LM_VT_STRING);
  assert_template_format_value_and_type("$MSG", "árvíztűrőtükörfúrógép", LM_VT_STRING);
  assert_template_format_value_and_type("$MESSAGE", "árvíztűrőtükörfúrógép", LM_VT_STRING);
  assert_template_format_value_and_type("$SOURCEIP", "10.11.12.13", LM_VT_STRING);
  assert_template_format_value_and_type("$RCPTID", "555", LM_VT_STRING);
  assert_template_format_value_and_type("$DESTIP", "127.0.0.5", LM_VT_STRING);
  assert_template_format_value_and_type("$DESTPORT", "6514", LM_VT_INTEGER);
  assert_template_format_value_and_type("$PROTO", "33", LM_VT_INTEGER);
  assert_template_format_value_and_type("$IP_PROTO", "4", LM_VT_INTEGER);

  assert_template_format_value_and_type("$SEQNUM", "999", LM_VT_STRING);
  assert_template_format_value_and_type("$CONTEXT_ID", "test-context-id", LM_VT_STRING);
  assert_template_format_value_and_type("$UNIQID", "cafebabe@000000000000022b", LM_VT_STRING);

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
  assert_template_format_with_escaping("$(echo ${APP.QVALUE})", TRUE, "\\\"value\\\"");
  assert_template_format_with_escaping("$(echo ${APP.QVALUE}) ${APP.QVALUE}", TRUE, "\\\"value\\\" \\\"value\\\"");
  assert_template_format_with_escaping("$(echo $(echo $(echo ${APP.QVALUE})))", TRUE, "\\\"value\\\"");
  assert_template_format_with_escaping("$(echo $(echo $(length ${APP.QVALUE})))", TRUE, "7");

  assert_template_format_with_escaping("${APP.QVALUE}", FALSE, "\"value\"");
  assert_template_format_with_escaping("${APP.QVALUE}", TRUE, "\\\"value\\\"");
  assert_template_format_with_escaping("$(if (\"${APP.VALUE}\" eq \"value\") \"${APP.QVALUE}\" \"${APP.QVALUE}\")",
                                       FALSE, "\"value\"");
  assert_template_format_with_escaping("$(if (\"${APP.VALUE}\" eq \"value\") \"${APP.QVALUE}\" \"${APP.QVALUE}\")",
                                       TRUE, "\\\"value\\\"");

  /* literal parts of the template are not escaped */
  assert_template_format_with_escaping("\"$(echo $(echo $(length ${APP.QVALUE})))\"", TRUE, "\"7\"");
  assert_template_format_with_escaping("\"almafa\"", TRUE, "\"almafa\"");
}

Test(template, test_user_template_function)
{
  LogTemplate *template;

  template = compile_template("this is a user-defined template function $DATE");
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
assert_template_trivial_value(const gchar *template_code, LogMessage *msg,
                              const gchar *expected_value, LogMessageValueType expected_type)
{
  LogTemplate *template = compile_template(template_code);
  LogMessageValueType type;

  cr_assert(log_template_is_trivial(template));

  const gchar *trivial_value = log_template_get_trivial_value_and_type(template, msg, NULL, &type);
  cr_assert_str_eq(trivial_value, expected_value);
  cr_assert_eq(type, expected_type);

  GString *formatted_value = g_string_sized_new(64);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq(trivial_value, formatted_value->str,
                   "Formatted and trivial value does not match: '%s' - '%s'", trivial_value, formatted_value->str);
  cr_assert_eq(type, expected_type,
               "Formatted and trivial type does not match: '%d' - '%d'", type, expected_type);

  g_string_free(formatted_value, TRUE);
  log_template_unref(template);
}


Test(template, test_single_values_and_literal_strings_are_considered_trivial)
{
  LogMessage *msg = create_sample_message();

  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  assert_template_trivial_value("", msg, "", LM_VT_STRING);
  assert_template_trivial_value(" ", msg, " ", LM_VT_STRING);
  assert_template_trivial_value("literal", msg, "literal", LM_VT_STRING);
  assert_template_trivial_value("$1", msg, "first-match", LM_VT_STRING);
  assert_template_trivial_value("$MSG", msg, "árvíztűrőtükörfúrógép", LM_VT_STRING);
  assert_template_trivial_value("$HOST", msg, "bzorp", LM_VT_STRING);
  assert_template_trivial_value("${APP.VALUE}", msg, "value", LM_VT_STRING);
  assert_template_trivial_value("${number1}", msg, "123", LM_VT_INTEGER);


  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);
  assert_template_trivial_value("${number1}", msg, "123", LM_VT_STRING);

  log_msg_unref(msg);
}

Test(template, test_get_trivial_handle_returns_the_handle_associated_with_the_trivial_template)
{
  LogTemplate *template;

  template = compile_template("$MESSAGE");
  cr_assert(log_template_is_trivial(template) == TRUE);
  cr_assert(log_template_get_trivial_value_handle(template) == LM_V_MESSAGE);
  log_template_unref(template);

  template = compile_template("$1");
  cr_assert(log_template_is_trivial(template) == TRUE);
  cr_assert(log_template_get_trivial_value_handle(template) == log_msg_get_match_handle(1));
  log_template_unref(template);

  template = compile_template("literal");
  cr_assert(log_template_is_trivial(template) == TRUE);
  cr_assert(log_template_get_trivial_value_handle(template) == LM_V_NONE);
  log_template_unref(template);
}

Test(template, test_invalid_templates_are_trivial)
{
  LogMessage *msg = create_sample_message();
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

  template = compile_escaped_template("$1");
  cr_assert_not(log_template_is_trivial(template), "Escaped template is not trivial");
  log_template_unref(template);

  template = compile_template("$1 $2");
  cr_assert_not(log_template_is_trivial(template), "Multi-element template is not trivial");
  log_template_unref(template);

  template = compile_template("$1 literal");
  cr_assert_not(log_template_is_trivial(template), "Multi-element template is not trivial");
  log_template_unref(template);

  template = compile_template("pre${1}");
  cr_assert_not(log_template_is_trivial(template), "Single-value template with preliminary text is not trivial");
  log_template_unref(template);

  template = compile_template("${MSG}@3");
  cr_assert_not(log_template_is_trivial(template), "Template referencing non-last context element is not trivial");
  log_template_unref(template);

  template = compile_template("$(echo test)");
  cr_assert_not(log_template_is_trivial(template), "Template functions are not trivial");
  log_template_unref(template);

  template = compile_template("$DATE");
  cr_assert_not(log_template_is_trivial(template), "Hard macros are not trivial");
  log_template_unref(template);
}

static void
assert_template_literal_value(const gchar *template_code, const gchar *expected_value)
{
  LogTemplate *template = compile_template(template_code);

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

  LogTemplate *template = compile_template("a b c d $MSG");
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

Test(template, test_result_of_concatenation_in_templates_are_typed_as_strings)
{
  assert_template_format_value_and_type("$HOST$PROGRAM", "bzorpsyslog-ng", LM_VT_STRING);
  assert_template_format_value_and_type("${number1}123", "123123", LM_VT_STRING);
  assert_template_format_value_and_type("123${number1}", "123123", LM_VT_STRING);
  assert_template_format_value_and_type("${number1}${number2}", "123456", LM_VT_STRING);
}

Test(template, test_literals_in_templates_are_typed_as_strings)
{
  assert_template_format_value_and_type("", "", LM_VT_STRING);
  assert_template_format_value_and_type("123", "123", LM_VT_STRING);
  assert_template_format_value_and_type("foobar", "foobar", LM_VT_STRING);
}

Test(template, test_single_element_typed_value_refs_are_typed_as_the_value)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  assert_template_format_value_and_type("${number1}", "123", LM_VT_INTEGER);
}

Test(template, test_single_element_typed_value_refs_with_escaping_are_typed_as_strings)
{
  assert_template_format_value_and_type_with_escaping("${number1}", TRUE, "123", LM_VT_STRING);
}

Test(template, test_default_values_are_typed_as_strings)
{
  assert_template_format_value_and_type("${unset:-foo}", "foo", LM_VT_STRING);
}

Test(template, test_type_hint_overrides_the_calculated_type)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  LogTemplate *template = compile_template("${number1}");

  GString *formatted_value = g_string_sized_new(64);
  LogMessage *msg = create_sample_message();
  LogMessageValueType type;

  /* no type-hint */
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("123", formatted_value->str);
  cr_assert_eq(type, LM_VT_INTEGER);

  cr_assert(log_template_set_type_hint(template, "float", NULL));
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("123", formatted_value->str);
  cr_assert_eq(type, LM_VT_DOUBLE);

  cr_assert(log_template_set_type_hint(template, "string", NULL));
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("123", formatted_value->str);
  cr_assert_eq(type, LM_VT_STRING);

  log_template_unref(template);
  template = compile_template("${HOST}");
  cr_assert(log_template_set_type_hint(template, "int64", NULL));
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("bzorp", formatted_value->str);
  cr_assert_eq(type, LM_VT_INTEGER);

  /* empty string with a type uses the type hint */
  log_template_unref(template);
  template = compile_template("");
  cr_assert(log_template_set_type_hint(template, "null", NULL));
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_NULL);

  log_template_unref(template);
  /* msgref out of range */
  template = compile_template("${HOST}@2");
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_STRING);

  log_msg_unref(msg);
  log_template_unref(template);
}

Test(template, test_log_template_compile_with_type_hint_sets_the_type_hint_member_too)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  LogTemplate *template = log_template_new(configuration, NULL);
  GError *error = NULL;
  gboolean result;

  cr_assert_eq(template->type_hint, LM_VT_NONE);

  result = log_template_compile_with_type_hint(template, "int64(1234)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_INTEGER);
  result = log_template_compile_with_type_hint(template, "string(1234)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_STRING);
  result = log_template_compile_with_type_hint(template, "list(foo,bar,baz)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_LIST);
  result = log_template_compile_with_type_hint(template, "generic-string", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_NONE);
  log_template_unref(template);
}

Test(template, test_log_template_compile_with_invalid_type_hint_resets_the_type_hint_to_none)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  LogTemplate *template = log_template_new(configuration, NULL);
  GError *error = NULL;
  gboolean result;

  cr_assert_eq(template->type_hint, LM_VT_NONE);

  result = log_template_compile_with_type_hint(template, "int64(1234)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_INTEGER);
  result = log_template_compile_with_type_hint(template, "unknown(generic-string)", &error);
  cr_assert_not(result);
  cr_assert_neq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_NONE);
  log_template_unref(template);
}

Test(template, test_log_template_with_escaping_produces_string_even_if_the_value_would_otherwise_be_numeric)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  GString *formatted_value = g_string_sized_new(64);
  LogMessage *msg = create_sample_message();
  LogMessageValueType type;

  LogTemplate *template = compile_template("$FACILITY_NUM");
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("19", formatted_value->str);
  cr_assert_eq(type, LM_VT_INTEGER);
  log_template_unref(template);

  template = compile_escaped_template("$FACILITY_NUM");
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("19", formatted_value->str);
  cr_assert_eq(type, LM_VT_STRING);
  log_template_unref(template);

  template = compile_template("$number1");
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("123", formatted_value->str);
  cr_assert_eq(type, LM_VT_INTEGER);
  log_template_unref(template);

  template = compile_escaped_template("$number1");
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("123", formatted_value->str);
  cr_assert_eq(type, LM_VT_STRING);
  log_template_unref(template);

  log_msg_unref(msg);
  g_string_free(formatted_value, TRUE);
}

Test(template, test_bytes_and_protobuf_types_are_rendered_when_necessary)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  LogTemplate *template = log_template_new(configuration, NULL);
  GError *error = NULL;
  gboolean result;

  GString *formatted_value = g_string_sized_new(64);
  LogMessage *msg = create_sample_message();
  LogMessageValueType type;

  result = log_template_compile(template, "$bytes", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_NULL);

  result = log_template_compile(template, "$protobuf", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_NULL);

  result = log_template_compile_with_type_hint(template, "bytes($bytes almafa)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_BYTES);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_eq(formatted_value->len, 11);
  cr_assert_eq(memcmp(formatted_value->str, "\0\1\2\3 almafa", 11), 0);
  cr_assert_eq(type, LM_VT_BYTES);

  result = log_template_compile_with_type_hint(template, "protobuf($protobuf almafa)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_PROTOBUF);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_eq(formatted_value->len, 11);
  cr_assert_eq(memcmp(formatted_value->str, "\4\5\6\7 almafa", 11), 0);
  cr_assert_eq(type, LM_VT_PROTOBUF);

  result = log_template_compile_with_type_hint(template, "bytes($protobuf)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_BYTES);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_BYTES);

  result = log_template_compile_with_type_hint(template, "protobuf($bytes)", &error);
  cr_assert(result);
  cr_assert_eq(error, NULL);
  cr_assert_eq(template->type_hint, LM_VT_PROTOBUF);
  log_template_format_value_and_type(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, formatted_value, &type);
  cr_assert_str_eq("", formatted_value->str);
  cr_assert_eq(type, LM_VT_PROTOBUF);

  log_template_unref(template);
  log_msg_unref(msg);
  g_string_free(formatted_value, TRUE);
}
