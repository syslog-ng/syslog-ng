/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2014 Balázs Scheidler <bazsi@balabit.hu>
 * Copyright (c) 2014 Viktor Tusa <tusa@balabit.hu>
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

#include "str-format.h"
#include <criterion/criterion.h>

Test(test_str_format, hex_string__single_byte__perfect)
{
  gchar expected_output[3] = "40";
  gchar output[3];
  gchar input[1] = "@";

  format_hex_string(input, sizeof(input), output, sizeof(output));

  cr_expect_str_eq(output, expected_output, "format_hex_string output does not match!");
}

Test(test_str_format, hex_string__two_bytes__perfect)
{
  gchar expected_output[5] = "4041";
  gchar output[5];
  gchar input[2] = "@A";

  format_hex_string(input, sizeof(input), output, sizeof(output));

  cr_expect_str_eq(output, expected_output, "format_hex_string output does not match with two bytes!");
}

Test(test_str_format, hex_string_with_delimiter__single_byte__perfect)
{
  gchar expected_output[3] = "40";
  gchar output[3];
  gchar input[1] = "@";

  format_hex_string_with_delimiter(input, sizeof(input), output, sizeof(output), ' ');

  cr_expect_str_eq(output, expected_output, "format_hex_string_with_delimiter output does not match!");
}

Test(test_str_format, hex_string_with_delimiter__two_bytes__perfect)
{
  gchar expected_output[6] = "40 41";
  gchar output[6];
  gchar input[2] = "@A";

  format_hex_string_with_delimiter(input, sizeof(input), output, sizeof(output), ' ');

  cr_expect_str_eq(output, expected_output,
                   "format_hex_string_with_delimiter output does not match in case of two bytes!");
}

static void
assert_scan_positive_int_in_field_value_equals(const gchar *input, gint field, gint expected_result)
{
  const gchar *buf = input;
  gint left = strlen(buf);
  gint result = expected_result + 1;

  gboolean success = scan_positive_int(&buf, &left, field >= 0 ? field : left, &result);

  cr_assert(success);
  if (field == -1 || field == strlen(input))
    {
      cr_assert(*buf == 0);
      cr_assert(left == 0);
    }
  else
    {
      cr_assert(buf[0] == input[field]);
      cr_assert(left == strlen(input) - field);
    }
  cr_assert_eq(result, expected_result);
}

static void
assert_scan_positive_int_value_equals(const gchar *input, gint expected_result)
{
  return assert_scan_positive_int_in_field_value_equals(input, -1, expected_result);
}

static void
assert_scan_positive_int_in_field_fails(const gchar *input, gint field)
{
  const gchar *buf = input;
  gint left = strlen(buf);
  gint result = 99;

  gboolean success = scan_positive_int(&buf, &left, field >= 0 ? field : left, &result);

  cr_assert_not(success);
}

static void
assert_scan_positive_int_fails(const gchar *input)
{
  return assert_scan_positive_int_in_field_fails(input, -1);
}


Test(test_str_format, scan_positive_int)
{
  assert_scan_positive_int_value_equals("1", 1);
  assert_scan_positive_int_value_equals("3", 3);
  assert_scan_positive_int_value_equals("11", 11);
  assert_scan_positive_int_value_equals("222", 222);

  assert_scan_positive_int_value_equals(" 3", 3);
  assert_scan_positive_int_value_equals("  3", 3);
  assert_scan_positive_int_value_equals("   3", 3);
  assert_scan_positive_int_in_field_value_equals("  3", 2, 0);
  assert_scan_positive_int_in_field_value_equals("  3", 3, 3);
  assert_scan_positive_int_in_field_value_equals("  31", 3, 3);

  assert_scan_positive_int_fails(" 3 ");
  assert_scan_positive_int_fails(" 0 ");
  assert_scan_positive_int_fails("a3");
  assert_scan_positive_int_fails("3a");

  /* string too short for field_length */
  assert_scan_positive_int_in_field_fails("31", 3);

  /* non-number in front */
  assert_scan_positive_int_in_field_fails("a31", 3);
  assert_scan_positive_int_in_field_fails("aaaaaaaa31", 3);
  assert_scan_positive_int_in_field_fails("31a", 3);
  assert_scan_positive_int_in_field_fails("31aaaaaaaa", 3);
}
