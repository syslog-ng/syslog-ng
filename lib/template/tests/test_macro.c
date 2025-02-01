/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan
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
#include "libtest/fake-time.h"

#include "template/macros.h"
#include "logmsg/logmsg.h"
#include "syslog-names.h"
#include "apphook.h"

static void
assert_macro_value(gint id, LogMessage *msg, const gchar *expected_value, LogMessageValueType expected_type)
{
  GString *resolved = g_string_new("");
  LogMessageValueType type;

  gboolean result = log_macro_expand_simple(id, msg, resolved, &type);
  cr_assert(result);
  cr_assert_str_eq(resolved->str, expected_value);
  cr_assert_eq(type, expected_type);

  g_string_free(resolved, TRUE);
}

Test(macro, test_facility)
{
  const gchar *facility_printable = "lpr";
  const gint facility_lpr = syslog_name_lookup_facility_by_name(facility_printable);

  LogMessage *msg = log_msg_new_empty();
  msg->pri = facility_lpr;

  assert_macro_value(M_FACILITY, msg, "lpr", LM_VT_STRING);

  log_msg_unref(msg);
}

Test(macro, test_date_week)
{
  LogMessage *msg = log_msg_new_empty();

  /* Wed Jan 1 11:20:50 GMT 2015 */
  fake_time(1420111250);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_WEEK, msg, "00", LM_VT_STRING);

  /* Thu Dec 31 11:20:50 GMT 2015 */
  fake_time(1451560850);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_WEEK, msg, "52", LM_VT_STRING);

  log_msg_unref(msg);
}

Test(macro, test_date_iso_week_testcases)
{
  LogMessage *msg = log_msg_new_empty();

  /* First week: Thu Jan 1 11:20:50 GMT 2015 */

  fake_time(1420111250);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_ISOWEEK, msg, "01", LM_VT_STRING);

  /* Last week, still in 2015: Thu Dec 31 11:20:50 GMT 2015 */
  fake_time(1451560850);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_ISOWEEK, msg, "53", LM_VT_STRING);

  /* Already 2016, but still the same week as the previous case: Fri Jan 1 11:20:50 GMT 2016 */
  fake_time(1451647250);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_ISOWEEK, msg, "53", LM_VT_STRING);

  /* Mon Jan 5 11:20:50 GMT 2015 */
  fake_time(1420456850);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);
  assert_macro_value(M_ISOWEEK, msg, "02", LM_VT_STRING);

  log_msg_unref(msg);
}

Test(macro, test_context_id_type_is_returned)
{
  GString *resolved = g_string_new("");
  LogMessage *msg = log_msg_new_empty();
  LogMessageValueType type;

  LogTemplateEvalOptions options = {NULL, LTZ_SEND, 5555, "5678", LM_VT_INTEGER};

  gboolean result = log_macro_expand(M_CONTEXT_ID, &options, msg, resolved, &type);
  cr_assert(result);
  cr_assert_str_eq(resolved->str, "5678");
  cr_assert_eq(type, LM_VT_INTEGER);

  g_string_free(resolved, TRUE);
  log_msg_unref(msg);
}

Test(macro, test__asterisk_returns_the_matches_as_a_list)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_match(msg, 1, "foo", -1);
  log_msg_set_match(msg, 2, "bar", -1);
  assert_macro_value(M__ASTERISK, msg, "foo,bar", LM_VT_LIST);
  log_msg_unref(msg);
}

Test(macro, test_ipv4_saddr_related_macros)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_saddr_ref(msg, g_sockaddr_inet_new("127.0.0.1", 2000));
  log_msg_set_daddr_ref(msg, g_sockaddr_inet_new("127.0.127.1", 2020));
  assert_macro_value(M_SOURCE_IP, msg, "127.0.0.1", LM_VT_STRING);
  assert_macro_value(M_SOURCE_PORT, msg, "2000", LM_VT_INTEGER);
  assert_macro_value(M_DEST_IP, msg, "127.0.127.1", LM_VT_STRING);
  assert_macro_value(M_DEST_PORT, msg, "2020", LM_VT_INTEGER);
  assert_macro_value(M_IP_PROTOCOL, msg, "4", LM_VT_INTEGER);
  log_msg_unref(msg);
}

#if SYSLOG_NG_ENABLE_IPV6
Test(macro, test_ipv6_saddr_related_macros)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_saddr_ref(msg, g_sockaddr_inet6_new("dead:beef::1", 2000));
  log_msg_set_daddr_ref(msg, g_sockaddr_inet6_new("::1", 2020));
  assert_macro_value(M_SOURCE_IP, msg, "dead:beef::1", LM_VT_STRING);
  assert_macro_value(M_DEST_IP, msg, "::1", LM_VT_STRING);
  assert_macro_value(M_DEST_PORT, msg, "2020", LM_VT_INTEGER);
  assert_macro_value(M_IP_PROTOCOL, msg, "6", LM_VT_INTEGER);
  log_msg_unref(msg);
}

Test(macro, test_ipv6_mapped_ipv4_saddr_related_macros)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_saddr_ref(msg, g_sockaddr_inet6_new("::FFFF:192.168.1.1", 2000));
  log_msg_set_daddr_ref(msg, g_sockaddr_inet6_new("::1", 2020));
  assert_macro_value(M_SOURCE_IP, msg, "192.168.1.1", LM_VT_STRING);
  assert_macro_value(M_DEST_IP, msg, "::1", LM_VT_STRING);
  assert_macro_value(M_DEST_PORT, msg, "2020", LM_VT_INTEGER);
  assert_macro_value(M_IP_PROTOCOL, msg, "4", LM_VT_INTEGER);
  log_msg_unref(msg);
}
#endif

void
setup(void)
{
  app_startup();

  setenv("TZ", "MET-1METDST", TRUE);
  tzset();
}

void
teardown(void)
{
  app_shutdown();
}

TestSuite(macro, .init = setup, .fini = teardown);
