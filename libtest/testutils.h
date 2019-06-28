/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Peter Gyorko
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

#ifndef LIBTEST_H_INCLUDED
#define LIBTEST_H_INCLUDED

#include <glib.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include "grab-logging.h"
#include "logmsg/logmsg.h"

#define PRETTY_STRING_FORMAT "%s%s%s"
#define PRETTY_STRING(str) ((str) ? "'" : "<"), ((str) ? (str) : "NULL"), ((str) ? "'" : ">")

#define PRETTY_NSTRING_FORMAT "%s%.*s%s"
#define PRETTY_NSTRING(str, len) ((str) ? "'" : "<"), len, ((str) ? (str) : "NULL"), ((str) ? "'" : ">")

#define array_count_guint32(array) ((array != NULL) ? ((guint32)(sizeof(array) / sizeof(guint32))) : 0)

#define gboolean_to_string(value) (value ? "TRUE" : "FALSE")

#define ASSERTION_ERROR(message) "%s:%d/%s\n  #       %s", \
                                 basename(__FILE__), __LINE__, __FUNCTION__, ((message) ? (message) : "")

gboolean assert_grabbed_messages_contain_non_fatal(const gchar *pattern, const gchar *error_message,
                                                   ...) G_GNUC_PRINTF(2, 3);

#define assert_grabbed_messages_contain(pattern, error_message, ...) (assert_grabbed_messages_contain_non_fatal(pattern, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

gchar **fill_string_array(gint number_of_elements, ...);

gboolean assert_guint16_non_fatal(guint16 actual, guint16 expected, const gchar *error_message,
                                  ...) G_GNUC_PRINTF(3, 4);
gboolean assert_gint64_non_fatal(gint64 actual, gint64 expected, const gchar *error_message,
                                 ...) G_GNUC_PRINTF(3, 4);
gboolean assert_guint64_non_fatal(guint64 actual, guint64 expected, const gchar *error_message,
                                  ...) G_GNUC_PRINTF(3, 4);
gboolean assert_nstring_non_fatal(const gchar *actual, gint actual_len, const gchar *expected, gint expected_len,
                                  const gchar *error_message, ...) G_GNUC_PRINTF(5, 6);
gboolean assert_guint32_array_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected,
                                        guint32 expected_length, const gchar *error_message, ...) G_GNUC_PRINTF(5, 6);
gboolean assert_string_array_non_fatal(gchar **actual, guint32 actual_length, gchar **expected, guint32 expected_length,
                                       const gchar *error_message, ...) G_GNUC_PRINTF(5, 6);
gboolean assert_gboolean_non_fatal(gboolean actual, gboolean expected, const gchar *error_message,
                                   ...) G_GNUC_PRINTF(3, 4);
gboolean assert_null_non_fatal(const void *pointer, const gchar *error_message, ...) G_GNUC_PRINTF(2, 3);
gboolean assert_not_null_non_fatal(void *pointer, const gchar *error_message, ...) G_GNUC_PRINTF(2, 3);
gboolean assert_no_error_non_fatal(GError *error, const gchar *error_message, ...) G_GNUC_PRINTF(2, 3);
gboolean assert_guint32_set_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected,
                                      guint32 expected_length, const gchar *error_message, ...) G_GNUC_PRINTF(5, 6);
gboolean assert_gpointer_non_fatal(gpointer actual, gpointer expected, const gchar *error_message,
                                   ...) G_GNUC_PRINTF(3, 4);
gboolean assert_msg_field_equals_non_fatal(LogMessage *msg, const gchar *field_name, const gchar *expected_value,
                                           gssize expected_value_len, const gchar *error_message, ...) G_GNUC_PRINTF(5, 6);
gboolean assert_msg_field_unset_non_fatal(LogMessage *msg, const gchar *field_name, const gchar *error_message,
                                          ...) G_GNUC_PRINTF(3, 4);
gboolean expect_not_reached(const gchar *error_message, ...) G_GNUC_PRINTF(1, 2);

#define assert_guint16(actual, expected, error_message, ...) (assert_guint16_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gint64(actual, expected, error_message, ...) (assert_gint64_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint64(actual, expected, error_message, ...) (assert_guint64_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gint(actual, expected, error_message, ...) (assert_gint64((gint64)actual, (gint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint(actual, expected, error_message, ...) (assert_guint64((guint64)actual, (guint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gint32(actual, expected, error_message, ...) (assert_gint64((gint64)actual, (gint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint32(actual, expected, error_message, ...) (assert_guint64((guint64)actual, (guint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_string(actual, expected, error_message, ...) (assert_nstring_non_fatal(actual, -1, expected, -1, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_nstring(actual, actual_len, expected, expected_len, error_message, ...) (assert_nstring_non_fatal(actual, actual_len, expected, expected_len, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define expect_nstring(actual, actual_len, expected, expected_len, error_message, ...) \
  assert_nstring_non_fatal(actual, actual_len, expected, expected_len, error_message, ##__VA_ARGS__)

#define assert_guint32_array(actual, actual_length, expected, expected_length, error_message, ...) ( \
    assert_guint32_array_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__)\
 ? 1 : (exit(1),0))

#define assert_string_array(actual, actual_length, expected, expected_length, error_message, ...) ( \
    assert_string_array_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__)\
 ? 1 : (exit(1),0))

#define assert_gboolean(actual, expected, error_message, ...) (assert_gboolean_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_true(value, error_message, ...) (assert_gboolean(value, TRUE, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_false(value, error_message, ...) (assert_gboolean(value, FALSE, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define expect_gboolean(actual, expected, error_message, ...) \
  assert_gboolean_non_fatal(actual, expected, error_message, ##__VA_ARGS__)
#define expect_true(value, error_message, ...) expect_gboolean(value, TRUE, error_message, ##__VA_ARGS__)
#define expect_false(value, error_message, ...) expect_gboolean(value, FALSE, error_message, ##__VA_ARGS__)

#define assert_null(pointer, error_message, ...) (assert_null_non_fatal((void *)pointer, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_not_null(pointer, error_message, ...) (assert_not_null_non_fatal((void *)pointer, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_no_error(error, error_message, ...) (assert_no_error_non_fatal(error, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint32_set(actual, actual_length, expected, expected_length, error_message, ...) (assert_guint32_set_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gpointer(actual, expected, error_message, ...) (assert_gpointer_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_msg_field_equals(msg, field_name, expected_value, expected_value_len, error_message, ...) (assert_msg_field_equals_non_fatal(msg, field_name, expected_value, expected_value_len, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_msg_field_unset(msg, field_name, error_message, ...) (assert_msg_field_unset_non_fatal(msg, field_name, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

extern GString *current_testcase_description;
extern const gchar *current_testcase_function;
extern gchar *current_testcase_file;

gboolean testutils_deinit(void);

#define testcase_begin(description_template, ...) \
    do { \
      if (current_testcase_description != NULL) \
        { \
          fprintf(stderr, "\nERROR: testcase_begin() called without testcase_end(); file='%s', function='%s'\n", \
                          current_testcase_file, current_testcase_function); \
          exit(1); \
        } \
      current_testcase_description = g_string_sized_new(0); \
      g_string_printf(current_testcase_description, description_template, ##__VA_ARGS__); \
      current_testcase_function = __FUNCTION__; \
      current_testcase_file = g_path_get_basename(__FILE__); \
    } while (0)

#define testcase_end() \
    do { \
      g_string_free(current_testcase_description, TRUE); \
      current_testcase_description = NULL; \
      current_testcase_function = NULL; \
      g_free(current_testcase_file); \
      current_testcase_file = NULL; \
    } while (0)

#endif
