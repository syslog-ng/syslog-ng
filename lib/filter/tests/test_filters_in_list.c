/*
 * Copyright (c) 2013, 2014 Balabit
 * Copyright (c) 2013, 2014 Gergely Nagy <algernon@balabit.hu>
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

#include <stdlib.h>
#include <glib.h>

#include "cfg.h"
#include "messages.h"
#include "syslog-names.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "plugin.h"
#include "filter/filter-in-list.h"

#include "testutils.h"

#define MSG_1 "<15>Sep  4 15:03:55 localhost test-program[3086]: some random message"
#define MSG_2 "<15>Sep  4 15:03:55 localhost foo[3086]: some random message"
#define MSG_3 "<15>Sep  4 15:03:55 192.168.1.1 foo[3086]: some random message"
#define MSG_LONG "<15>Sep  4 15:03:55 test-hostAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA foo[3086]: some random message"

#define LIST_FILE_DIR "%s/lib/filter/tests/filters-in-list/"

static MsgFormatOptions parse_options;

gboolean
evaluate_testcase(const gchar *msg,
                  FilterExprNode *filter_node)
{
  LogMessage *log_msg;
  gboolean result;

  assert_not_null(filter_node, "Constructing an in-list filter");
  log_msg = log_msg_new(msg, strlen(msg), &parse_options);
  result = filter_expr_eval(filter_node, log_msg);

  log_msg_unref(log_msg);
  filter_expr_unref(filter_node);
  return result;
}

void
test_filter_returns_false_when_list_is_empty(const char *top_srcdir)
{
  gchar *list_file_with_zero_lines = g_strdup_printf(LIST_FILE_DIR "empty.list", top_srcdir);

  assert_gboolean(evaluate_testcase(MSG_1, filter_in_list_new(list_file_with_zero_lines, "PROGRAM")),
                  FALSE,
                  "in-list filter matches");

  g_free(list_file_with_zero_lines);
}

void
test_string_searched_for_is_not_in_the_list(const char *top_srcdir)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_2, filter_in_list_new(list_file_with_one_line, "PROGRAM")),
                  FALSE,
                  "in-list filter matches");
  g_free(list_file_with_one_line);
}

void
test_given_macro_is_not_available_in_this_message(const char *top_srcdir)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_2, filter_in_list_new(list_file_with_one_line, "FOO_MACRO")),
                  FALSE,
                  "in-list filter matches");
  g_free(list_file_with_one_line);
}

void
test_list_file_doesnt_exist(const char *top_srcdir)
{
  gchar *list_file_which_doesnt_exist = g_strdup_printf(LIST_FILE_DIR "notexisting.list", top_srcdir);
  assert_null(filter_in_list_new(list_file_which_doesnt_exist, "PROGRAM"),
              "in-list filter should fail, when the list file does not exist");
  g_free(list_file_which_doesnt_exist);
}

void
test_list_file_contains_only_one_line(const char *top_srcdir)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_1, filter_in_list_new(list_file_with_one_line, "PROGRAM")),
                  TRUE,
                  "in-list filter matches");
  g_free(list_file_with_one_line);
}

void
test_list_file_contains_lot_of_lines(const char *top_srcdir)
{
  gchar *list_file_which_has_a_lot_of_lines = g_strdup_printf(LIST_FILE_DIR "lot_of_lines.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_1, filter_in_list_new(list_file_which_has_a_lot_of_lines, "PROGRAM")),
                  TRUE,
                  "in-list filter matches");
  g_free(list_file_which_has_a_lot_of_lines);
}

void
test_filter_with_ip_address(const char *top_srcdir)
{
  gchar *list_file_with_ip_address = g_strdup_printf(LIST_FILE_DIR "ip.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_3, filter_in_list_new(list_file_with_ip_address, "HOST")),
                  TRUE,
                  "in-list filter matches");
  g_free(list_file_with_ip_address);
}

void
test_filter_with_long_line(const char *top_srcdir)
{
  gchar *list_file_with_long_line = g_strdup_printf(LIST_FILE_DIR "long_line.list", top_srcdir);
  assert_gboolean(evaluate_testcase(MSG_LONG, filter_in_list_new(list_file_with_long_line, "HOST")),
                  TRUE,
                  "in-list filter matches");
  g_free(list_file_with_long_line);
}

void
run_testcases(const char *top_srcdir)
{
  test_filter_returns_false_when_list_is_empty(top_srcdir);
  test_string_searched_for_is_not_in_the_list(top_srcdir);
  test_given_macro_is_not_available_in_this_message(top_srcdir);
  test_list_file_doesnt_exist(top_srcdir);
  test_list_file_contains_only_one_line(top_srcdir);
  test_list_file_contains_lot_of_lines(top_srcdir);
  test_filter_with_ip_address(top_srcdir);
  test_filter_with_long_line(top_srcdir);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  char *top_srcdir = TOP_SRCDIR;

  app_startup();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  assert_not_null(top_srcdir, "The $top_srcdir environment variable MUST NOT be empty!");

  run_testcases(top_srcdir);

  app_shutdown();

  return 0;
}
