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

#include "template/macros.h"
#include "logmsg/logmsg.h"
#include "syslog-names.h"
#include "fake-time.h"


Test(macro, test_facility)
{
  const gchar *facility_printable = "lpr";
  const gint facility_lpr = syslog_name_lookup_facility_by_name(facility_printable);

  LogMessage *msg = log_msg_new_empty();
  msg->pri = facility_lpr;

  GString *resolved = g_string_new("");
  gboolean result = log_macro_expand_simple(resolved, M_FACILITY, msg);

  cr_assert(result);
  cr_assert_str_eq(resolved->str, "lpr");

  log_msg_unref(msg);
  g_string_free(resolved, TRUE);
}

Test(macro, test_date_week)
{
  LogMessage *msg = log_msg_new_empty();

  /* Wed Jan 1 11:20:50 GMT 2015 */
  fake_time(1420111250);
  unix_time_set_now(&msg->timestamps[LM_TS_STAMP]);

  GString *resolved = g_string_new("");
  gboolean result = log_macro_expand_simple(resolved, M_WEEK, msg);

  cr_assert(result);
  cr_assert_str_eq(resolved->str, "00");

  log_msg_unref(msg);
  g_string_free(resolved, TRUE);
}

