/*
 * Copyright (c) 2018 Balabit
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

#include "messages.h"
#include "apphook.h"
#include "logmsg/logmsg.h"

#include <errno.h>

TestSuite(test_messages, .init = app_startup, .fini = app_shutdown);

void simple_test_asserter(LogMessage *msg)
{
  cr_assert_str_eq(log_msg_get_value_by_name(msg, "MESSAGE", NULL), "simple test;");
  log_msg_unref(msg);
}

Test(test_messages, simple_test)
{
  msg_set_post_func(simple_test_asserter);
  msg_error("simple test");
}

void test_errno_asserter(LogMessage *msg)
{
  gchar expected_message[100];
  g_snprintf(expected_message, sizeof(expected_message), "test errno; error='%s (%d)'", strerror(5), 5);
  cr_assert_str_eq(log_msg_get_value_by_name(msg, "MESSAGE", NULL), expected_message);
  log_msg_unref(msg);
}

Test(test_messages, test_errno)
{
  errno = 5;
  msg_set_post_func(test_errno_asserter);
  msg_error("test errno",
            evt_tag_error("error"));
}

void test_errno_capture_asserter(LogMessage *msg)
{
  gchar pattern[100];
  g_snprintf(pattern, sizeof(pattern), "error='%s (%d)'", strerror(9), 9);
  const gchar *msg_content = log_msg_get_value_by_name(msg, "MESSAGE", NULL);
  cr_assert(g_strstr_len(msg_content, -1, pattern), "searching: >>%s<< in >>%s<<", pattern, msg_content);
  log_msg_unref(msg);
}

Test(test_messages, test_errno_capture)
{
  errno = 9;
  msg_set_post_func(test_errno_capture_asserter);
  msg_error("test errno capture", (errno=10, evt_tag_error("error")));
}

Test(test_messages, test_msg_set_log_level_sets_flags_correctly)
{
  verbose_flag = FALSE;
  debug_flag = FALSE;
  trace_flag = FALSE;

  msg_set_log_level(0);
  cr_assert_not(verbose_flag);
  cr_assert_not(debug_flag);
  cr_assert_not(trace_flag);
  cr_assert_eq(msg_get_log_level(), 0);

  msg_set_log_level(1);
  cr_assert(verbose_flag);
  cr_assert_not(debug_flag);
  cr_assert_not(trace_flag);
  cr_assert_eq(msg_get_log_level(), 1);

  msg_set_log_level(2);
  cr_assert(verbose_flag);
  cr_assert(debug_flag);
  cr_assert_not(trace_flag);
  cr_assert_eq(msg_get_log_level(), 2);

  msg_set_log_level(3);
  cr_assert(verbose_flag);
  cr_assert(debug_flag);
  cr_assert(trace_flag);
  cr_assert_eq(msg_get_log_level(), 3);

  /* this does nothing */
  msg_set_log_level(-1);
  cr_assert(verbose_flag);
  cr_assert(debug_flag);
  cr_assert(trace_flag);
  cr_assert_eq(msg_get_log_level(), 3);

  msg_set_log_level(0);
  cr_assert_not(verbose_flag);
  cr_assert_not(debug_flag);
  cr_assert_not(trace_flag);
  cr_assert_eq(msg_get_log_level(), 0);

  /* this does nothing */
  msg_set_log_level(-1);
  cr_assert_not(verbose_flag);
  cr_assert_not(debug_flag);
  cr_assert_not(trace_flag);
  cr_assert_eq(msg_get_log_level(), 0);
}

Test(test_messages, test_msg_apply_config_log_level_sets_log_level_if_unset)
{
  msg_apply_config_log_level(3);
  cr_assert_eq(msg_get_log_level(), 3);
}

Test(test_messages, test_msg_apply_config_log_level_is_ignored_if_already_set)
{
  msg_apply_cmdline_log_level(1);
  msg_apply_config_log_level(3);
  cr_assert_eq(msg_get_log_level(), 1);
}
