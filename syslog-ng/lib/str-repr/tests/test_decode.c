/*
 * Copyright (c) 2015-2016 Balabit
 * Copyright (c) 2015-2016 Balázs Scheidler
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
#include "str-repr/decode.h"
#include <criterion/criterion.h>

GString *value;

static void
assert_decode_equals(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");
  const gchar *end;

  cr_assert(str_repr_decode(str, input, &end), "Decode operation failed while success was expected, input=%s", input);
  cr_assert_str_eq(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

static void
assert_decode_equals_and_fails(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");
  const gchar *end;

  cr_assert_not(str_repr_decode(str, input, &end), "Decode operation succeeded while failure was expected, input=%s",
                input);
  cr_assert_str_eq(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

static gboolean
_match_three_tabs_as_delimiter(const gchar *cur, const gchar **new_cur, gpointer user_data)
{
  *new_cur = cur + 3;
  return (strncmp(cur, "\t\t\t", 3) == 0);
}

static void
assert_decode_with_three_tabs_as_delimiter_equals(const gchar *input, const gchar *expected)
{
  StrReprDecodeOptions options =
  {
    .match_delimiter = _match_three_tabs_as_delimiter,
    0
  };
  GString *str = g_string_new("");
  const gchar *end;

  str_repr_decode_with_options(str, input, &end, &options);
  cr_assert_str_eq(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

Test(decode, test_decode_unquoted_strings)
{
  assert_decode_equals("a", "a");
  assert_decode_equals("alma", "alma");
  assert_decode_equals("al ma", "al");
}

Test(decode, test_decode_double_quoted_strings)
{
  assert_decode_equals("\"al ma\"", "al ma");
  /* embedded quote */
  assert_decode_equals("\"\\\"value1\"", "\"value1");
  /* control sequences */
  assert_decode_equals("\"\\b \\f \\n \\r \\t \\\\\"", "\b \f \n \r \t \\");
  /* unknown backslash escape is left as is */
  assert_decode_equals("\"\\p\"", "\\p");
}

Test(decode, test_decode_apostrophe_quoted_strings)
{
  assert_decode_equals("'al ma'", "al ma");
  /* embedded quote */
  assert_decode_equals("'\\'value1'", "'value1");

  /* control sequences */
  assert_decode_equals("'\\b \\f \\n \\r \\t \\\\'", "\b \f \n \r \t \\");
  /* unknown backslash escape is left as is */
  assert_decode_equals("'\\p\'", "\\p");
}

Test(decode, test_decode_malformed_strings)
{
  assert_decode_equals_and_fails("'alma", "'alma");
  assert_decode_equals_and_fails("\"alma", "\"alma");
  assert_decode_equals("alma'", "alma'");
  assert_decode_equals("alma\"", "alma\"");
  assert_decode_equals("alma\"korte", "alma\"korte");
  assert_decode_equals("alma\"korte\"", "alma\"korte\"");
  assert_decode_equals_and_fails("'alma'@korte", "'alma'@korte");
}

Test(decode, test_decode_delimited_strings)
{
  assert_decode_with_three_tabs_as_delimiter_equals("alma\t\t\tkorte", "alma");

  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'", "alma\t\t\tkorte");
  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'\t\t", "'alma\t\t\tkorte'\t\t");
  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'\t\t\t", "alma\t\t\tkorte");
  assert_decode_with_three_tabs_as_delimiter_equals("alma\t\t", "alma\t\t");

  assert_decode_with_three_tabs_as_delimiter_equals("\t\t\tfoobar", "");
}
