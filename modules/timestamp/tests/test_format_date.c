/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "logmsg/logmsg.h"
#include "timeutils/cache.h"
#include "apphook.h"
#include "scratch-buffers.h"
#include "cfg.h"

void
setup(void)
{
  app_startup();
  setenv("TZ", "CET", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "timestamp");
}

void
teardown(void)
{
  deinit_template_tests();
  scratch_buffers_explicit_gc();
  app_shutdown();
}

static void
_log_msg_set_recvd_time(LogMessage *msg, time_t t)
{
  msg->timestamps[LM_TS_STAMP].ut_sec = t;
  msg->timestamps[LM_TS_STAMP].ut_usec = 0;
  msg->timestamps[LM_TS_STAMP].ut_gmtoff = get_local_timezone_ofs(t);
}

TestSuite(format_date, .init = setup, .fini = teardown);

Test(format_date, test_format_date_without_argument_takes_the_timestamp_and_formats_it_using_strftime)
{
  LogMessage *msg = log_msg_new_empty();

  _log_msg_set_recvd_time(msg, 1667500613);
  assert_template_format_msg("$(format-date %Y-%m-%dT%H:%M:%S)", "2022-11-03T19:36:53", msg);

  log_msg_unref(msg);
}

Test(format_date, test_format_date_with_a_macro_argument_uses_that_value)
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value_by_name(msg, "custom_timestamp", "1667500613", -1);
  assert_template_format_msg("$(format-date %Y-%m-%dT%H:%M:%S ${custom_timestamp})", "2022-11-03T19:36:53", msg);

  log_msg_unref(msg);
}

Test(format_date, test_format_date_with_timestamp_argument_formats_it_using_strftime)
{
  assert_template_format("$(format-date %Y-%m-%dT%H:%M:%S 1667500613)", "2022-11-03T19:36:53");
}

Test(format_date, test_format_date_with_timestamp_argument_using_fractions_and_timezone_works)
{
  assert_template_format("$(format-date %Y-%m-%dT%H:%M:%S 1667500613.613+05:00)", "2022-11-03T23:36:53");
}

Test(format_date, test_format_date_with_time_zone_option_overrrides_timezone)
{
  assert_template_format("$(format-date --time-zone PST8PDT %Y-%m-%dT%H:%M:%S 1667500613)", "2022-11-03T11:36:53");
}
