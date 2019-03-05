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

#include <criterion/criterion.h>

#define MSG_1 "<15>Sep  4 15:03:55 localhost test-program[3086]: some random message"
#define MSG_2 "<15>Sep  4 15:03:55 localhost foo[3086]: some random message"
#define MSG_3 "<15>Sep  4 15:03:55 192.168.1.1 foo[3086]: some random message"
#define MSG_LONG "<15>Sep  4 15:03:55 test-hostAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA foo[3086]: some random message"

#define LIST_FILE_DIR "%s/lib/filter/tests/filters-in-list/"

static MsgFormatOptions parse_options;
char *top_srcdir = TOP_SRCDIR;

gboolean
evaluate_testcase(const gchar *msg,
                  FilterExprNode *filter_node)
{
  LogMessage *log_msg;
  gboolean result;

  cr_assert_not_null(filter_node, "Constructing an in-list filter");
  log_msg = log_msg_new(msg, strlen(msg), NULL, &parse_options);
  result = filter_expr_eval(filter_node, log_msg);

  log_msg_unref(log_msg);
  filter_expr_unref(filter_node);
  return result;
}

Test(template_filters, test_filter_returns_false_when_list_is_empty)
{
  gchar *list_file_with_zero_lines = g_strdup_printf(LIST_FILE_DIR "empty.list", top_srcdir);

  cr_assert_not(evaluate_testcase(MSG_1, filter_in_list_new(list_file_with_zero_lines, "PROGRAM")),
                "in-list filter matches");

  g_free(list_file_with_zero_lines);
}

Test(template_filters, test_string_searched_for_is_not_in_the_list)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  cr_assert_not(evaluate_testcase(MSG_2, filter_in_list_new(list_file_with_one_line, "PROGRAM")),
                "in-list filter matches");
  g_free(list_file_with_one_line);
}

Test(template_filters, test_given_macro_is_not_available_in_this_message)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  cr_assert_not(evaluate_testcase(MSG_2, filter_in_list_new(list_file_with_one_line, "FOO_MACRO")),
                "in-list filter matches");
  g_free(list_file_with_one_line);
}

Test(template_filters, test_list_file_doesnt_exist)
{
  gchar *list_file_which_doesnt_exist = g_strdup_printf(LIST_FILE_DIR "notexisting.list", top_srcdir);
  cr_assert_null(filter_in_list_new(list_file_which_doesnt_exist, "PROGRAM"),
                 "in-list filter should fail, when the list file does not exist");
  g_free(list_file_which_doesnt_exist);
}

Test(template_filters, test_list_file_contains_only_one_line)
{
  gchar *list_file_with_one_line = g_strdup_printf(LIST_FILE_DIR "test.list", top_srcdir);
  cr_assert(evaluate_testcase(MSG_1, filter_in_list_new(list_file_with_one_line, "PROGRAM")),
            "in-list filter matches");
  g_free(list_file_with_one_line);
}

Test(template_filters, test_list_file_contains_lot_of_lines)
{
  gchar *list_file_which_has_a_lot_of_lines = g_strdup_printf(LIST_FILE_DIR "lot_of_lines.list", top_srcdir);
  cr_assert(evaluate_testcase(MSG_1, filter_in_list_new(list_file_which_has_a_lot_of_lines, "PROGRAM")),
            "in-list filter matches");
  g_free(list_file_which_has_a_lot_of_lines);
}

Test(template_filters, test_filter_with_ip_address)
{
  gchar *list_file_with_ip_address = g_strdup_printf(LIST_FILE_DIR "ip.list", top_srcdir);
  cr_assert(evaluate_testcase(MSG_3, filter_in_list_new(list_file_with_ip_address, "HOST")),
            "in-list filter matches");
  g_free(list_file_with_ip_address);
}

Test(template_filters, test_filter_with_long_line)
{
  gchar *list_file_with_long_line = g_strdup_printf(LIST_FILE_DIR "long_line.list", top_srcdir);
  cr_assert(evaluate_testcase(MSG_LONG, filter_in_list_new(list_file_with_long_line, "HOST")),
            "in-list filter matches");
  g_free(list_file_with_long_line);
}

static void
setup(void)
{
  app_startup();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  cr_assert_not_null(top_srcdir, "The $top_srcdir environment variable MUST NOT be empty!");
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(template_filters, .init = setup, .fini = teardown);
