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

#include "testutils.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static gboolean testutils_global_success = TRUE;
GString *current_testcase_description = NULL;
const gchar *current_testcase_function = NULL;
gchar *current_testcase_file = NULL;
GList *internal_messages = NULL;

static void
print_failure(const gchar *custom_template, va_list custom_args, const gchar *assertion_failure_template, ...)
{
  testutils_global_success = FALSE;
  va_list assertion_failure_args;
  fprintf(stderr, "\n  ###########################################################################\n  #\n");
  fprintf(stderr,   "  # FAIL: ASSERTION FAILED");
  if (custom_template != NULL)
    {
      fprintf(stderr, ": ");
      vfprintf(stderr, custom_template, custom_args);
    }
  fprintf(stderr, "; ");

  va_start(assertion_failure_args, assertion_failure_template);
  vfprintf(stderr, assertion_failure_template, assertion_failure_args);
  va_end(assertion_failure_args);

  fprintf(stderr, "\n");

  if (current_testcase_description != NULL)
    {
      fprintf(stderr, "  #\n");
      fprintf(stderr, "  # Test case: %s/%s()\n", current_testcase_file, current_testcase_function);
      fprintf(stderr, "  #\n");
      fprintf(stderr, "  #     %s\n", current_testcase_description->str);
    }

  fprintf(stderr, "  #\n  ###########################################################################\n\n");
}

static void
grab_message(LogMessage *msg)
{
  internal_messages = g_list_append(internal_messages, msg);
}

void
reset_grabbed_messages(void)
{
  g_list_foreach(internal_messages, (GFunc) log_msg_unref, NULL);
  g_list_free(internal_messages);
  internal_messages = NULL;
}

void
start_grabbing_messages(void)
{
  reset_grabbed_messages();
  msg_set_post_func(grab_message);
}

void
display_grabbed_messages(void)
{
  GList *l;

  if (internal_messages)
    {
      fprintf(stderr, "  # Grabbed internal messages follow:\n");
      for (l = internal_messages; l; l = l->next)
        {
          LogMessage *msg = (LogMessage *) l->data;
          const gchar *msg_text = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

          fprintf(stderr, "  #\t%s\n", msg_text);
        }
    }
  else
    {
      fprintf(stderr, "  # No internal messeges grabbed!\n");
    }
}

void
stop_grabbing_messages(void)
{
  msg_set_post_func(NULL);
}

gboolean
assert_grabbed_messages_contain_non_fatal(const gchar *pattern, const gchar *error_message, ...)
{
  GList *l;
  va_list args;

  for (l = internal_messages; l; l = l->next)
    {
      LogMessage *msg = (LogMessage *) l->data;
      const gchar *msg_text = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

      if (strstr(msg_text, pattern))
        {
          return TRUE;
        }
    }

  va_start(args, error_message);
  print_failure(error_message, args, "no grabbed message contains the pattern=%s", pattern);
  va_end(args);

  display_grabbed_messages();

  return FALSE;
}

gchar **
fill_string_array(gint number_of_elements, ...)
{
  va_list args;
  gint i;
  gchar **result;

  result = g_new(gchar *, number_of_elements);

  va_start(args, number_of_elements);
  for (i = 0; i < number_of_elements; ++i)
    {
      result[i] = va_arg(args, char *);
    }

  va_end(args);
  return result;
}

gboolean
assert_guint16_non_fatal(guint16 actual, guint16 expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%d, expected=%d", actual, expected);
  va_end(args);

  return FALSE;
}

gboolean
assert_gint64_non_fatal(gint64 actual, gint64 expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%lld, expected=%lld", actual, expected);
  va_end(args);

  return FALSE;
}

gboolean
assert_guint64_non_fatal(guint64 actual, guint64 expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%llu, expected=%llu", actual, expected);
  va_end(args);

  return FALSE;
}

gboolean
assert_gdouble_non_fatal(gdouble actual, gdouble expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%f, expected=%f", actual, expected);
  va_end(args);

  return FALSE;
}

static gboolean
assert_nstring_non_fatal_va(const gchar *actual, gint actual_len, const gchar *expected, gint expected_len,
                            const gchar *error_message, va_list args)
{
  if (expected == NULL && actual == NULL)
    return TRUE;

  if (actual && actual_len < 0)
    actual_len = strlen(actual);

  if (expected && expected_len < 0)
    expected_len = strlen(expected);

  if (actual_len == expected_len &&
      actual != NULL && expected != NULL &&
      memcmp(actual, expected, actual_len) == 0)
    return TRUE;

  print_failure(error_message, args,
                " actual_length=%d expected_length=%d,\n"
                "  #  actual=   " PRETTY_NSTRING_FORMAT ",\n"
                "  #  expected= " PRETTY_NSTRING_FORMAT,
                actual_len, expected_len,
                PRETTY_NSTRING(actual, actual_len),
                PRETTY_NSTRING(expected, expected_len));

  return FALSE;
}

gboolean
assert_nstring_non_fatal(const gchar *actual, gint actual_len, const gchar *expected, gint expected_len,
                         const gchar *error_message, ...)
{
  va_list args;
  gboolean result;

  va_start(args, error_message);

  result = assert_nstring_non_fatal_va(actual, actual_len, expected, expected_len, error_message, args);

  va_end(args);
  return result;
}


static gboolean
compare_arrays_trivially(void *actual, guint32 actual_length,
                         void *expected, guint32 expected_length,
                         const gchar *error_message_template, va_list error_message_args)
{
  if (expected_length != actual_length)
    {
      print_failure(error_message_template, error_message_args, "actual_length=%u, expected_length=%u", actual_length,
                    expected_length);
      return FALSE;
    }

  if (expected_length > 0 && actual == NULL)
    {
      print_failure(error_message_template, error_message_args, "actual=NULL, expected_length=%u", expected_length);
      return FALSE;
    }

  return TRUE;
}

gboolean
assert_guint32_array_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected, guint32 expected_length,
                               const gchar *error_message, ...)
{
  va_list args;
  gboolean assertion_ok = TRUE;
  guint32 i;

  va_start(args, error_message);

  assertion_ok = compare_arrays_trivially((void *)actual, actual_length, (void *)expected, expected_length, error_message,
                                          args);
  if (assertion_ok)
    {
      for (i = 0; i < expected_length; ++i)
        {
          if (expected[i] != actual[i])
            {
              print_failure(error_message, args, "actual=%u, expected=%u, index=%u", actual[i], expected[i], i);
              assertion_ok = FALSE;
              break;
            }
        }
    }

  va_end(args);

  return assertion_ok;
}

/* NOTE: this does the same as g_strcmp0(), but we use an older glib, which lacks this function */
static gboolean
are_strings_equal(gchar *a, gchar *b)
{
  if (a == NULL && b == NULL)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  return strcmp(a, b) == 0;
}

gboolean
assert_string_array_non_fatal(gchar **actual, guint32 actual_length, gchar **expected, guint32 expected_length,
                              const gchar *error_message, ...)
{
  va_list args;
  gboolean assertion_ok = TRUE;
  guint32 i;

  va_start(args, error_message);

  assertion_ok = compare_arrays_trivially((void *)actual, actual_length, (void *)expected, expected_length, error_message,
                                          args);
  if (assertion_ok)
    {
      for (i = 0; i < expected_length; ++i)
        {
          if (!are_strings_equal(actual[i], expected[i]))
            {
              print_failure(error_message, args, "actual=" PRETTY_STRING_FORMAT ", expected=" PRETTY_STRING_FORMAT ", index=%u",
                            PRETTY_STRING(actual[i]), PRETTY_STRING(expected[i]), i);
              assertion_ok = FALSE;
              break;
            }
        }
    }

  va_end(args);

  return assertion_ok;
}

gboolean
expect_not_reached(const gchar *error_message, ...)
{
  va_list args;

  va_start(args, error_message);
  print_failure(error_message, args, "execution was not expected to reach this point");
  va_end(args);

  return FALSE;
}

gboolean
assert_gboolean_non_fatal(gboolean actual, gboolean expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%s, expected=%s", gboolean_to_string(actual), gboolean_to_string(expected));
  va_end(args);

  return FALSE;
}

gboolean
assert_null_non_fatal_va(const void *pointer, const gchar *error_message, va_list args)
{
  if (pointer == NULL)
    return TRUE;

  print_failure(error_message, args, "Pointer expected to be NULL; pointer=%llx", (guint64)pointer);

  return FALSE;
}

gboolean
assert_null_non_fatal(const void *pointer, const gchar *error_message, ...)
{
  va_list args;

  va_start(args, error_message);
  gboolean success = assert_null_non_fatal_va(pointer, error_message, args);
  va_end(args);

  return success;
}

gboolean
assert_not_null_non_fatal_va(void *pointer, const gchar *error_message, va_list args)
{
  if (pointer != NULL)
    return TRUE;

  print_failure(error_message, args, "Unexpected NULL pointer");

  return FALSE;
}

gboolean
assert_not_null_non_fatal(void *pointer, const gchar *error_message, ...)
{
  va_list args;

  va_start(args, error_message);
  gboolean success = assert_not_null_non_fatal_va(pointer, error_message, args);
  va_end(args);

  return success;
}

gboolean
assert_no_error_non_fatal(GError *error, const gchar *error_message, ...)
{
  va_list args;

  if (error == NULL)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "GError expected to be NULL; message='%s'", error->message);
  va_end(args);

  return FALSE;
}

static int
cmp_guint32(const void *a, const void *b)
{
  return (*(guint32 *)a - *(guint32 *)b);
}

gboolean
assert_guint32_set_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected, guint32 expected_length,
                             const gchar *error_message, ...)
{
  va_list args;
  gboolean ret;

  if (actual_length != expected_length)
    {
      va_start(args, error_message);
      print_failure(error_message, args, "actual_length='%d', expected_length='%d'", actual_length, expected_length);
      va_end(args);
      return FALSE;
    }

  qsort(actual, actual_length, sizeof(guint32), cmp_guint32);
  qsort(expected, expected_length, sizeof(guint32), cmp_guint32);

  va_start(args, error_message);
  ret = assert_guint32_array(actual, actual_length, expected, expected_length, error_message, args);
  va_end(args);

  return ret;
}

gboolean
assert_gpointer_non_fatal(gpointer actual, gpointer expected, const gchar *error_message, ...)
{
  va_list args;

  if (actual == expected)
    return TRUE;

  va_start(args, error_message);
  print_failure(error_message, args, "actual=%x, expected=%x", actual, expected);
  va_end(args);

  return FALSE;
}

gboolean
assert_msg_field_equals_non_fatal(LogMessage *msg, const gchar *field_name, const gchar *expected_value,
                                  gssize expected_value_len, const gchar *error_message, ...)
{
  gssize actual_value_len;
  const gchar *actual_value;
  va_list args;
  gboolean result;

  if (expected_value_len < 0)
    expected_value_len = strlen(expected_value);

  NVHandle handle = log_msg_get_value_handle(field_name);
  actual_value = log_msg_get_value(msg, handle, &actual_value_len);

  va_start(args, error_message);
  result = assert_nstring_non_fatal_va(actual_value, actual_value_len, expected_value, expected_value_len, error_message,
                                       args);
  va_end(args);

  return result;
}

gboolean
assert_msg_field_unset_non_fatal(LogMessage *msg, const gchar *field_name, const gchar *error_message, ...)
{
  gssize actual_value_len;
  const gchar *actual_value;
  va_list args;

  NVHandle handle = log_msg_get_value_handle(field_name);
  actual_value = log_msg_get_value_if_set(msg, handle, &actual_value_len);

  va_start(args, error_message);

  gboolean success = assert_null_non_fatal_va(actual_value, error_message, args);

  va_end(args);
  return success;
}

gboolean
testutils_deinit(void)
{
  return testutils_global_success;
}
