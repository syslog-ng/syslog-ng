/*
 * Copyright (c) 2016 Balabit
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
#include "str-utils.h"

/* this macro defines which strchr() implementation we are testing. It is
 * extracted as a macro in order to make it easy to switch strchr()
 * implementation, provided there would be more.
 */

#define strchr_under_test _strchr_optimized_for_single_char_haystack

static void
assert_strchr_is_null(const gchar *str, int c)
{
  assert_null(strchr_under_test(str, c), "expected a NULL return");
}

static void
assert_strchr_finds_character_at(const gchar *str, int c, int ofs)
{
  char *result = strchr_under_test(str, c);

  assert_not_null(result, "expected a non-NULL return");
  assert_true(result - str <= strlen(str), "Expected the strchr() return value to point into the input string or the terminating NUL, it points past the NUL");
  assert_true(result >= str, "Expected the strchr() return value to point into the input string or the terminating NUL, it points before the start of the string");
  assert_gint((result - str), ofs, "Expected the strchr() return value to point right to the specified offset");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  assert_strchr_is_null("", 'x');
  assert_strchr_is_null("a", 'x');
  assert_strchr_is_null("abc", 'x');

  assert_strchr_finds_character_at("", '\0', 0);
  assert_strchr_finds_character_at("a", 'a', 0);
  assert_strchr_finds_character_at("a", '\0', 1);
  assert_strchr_finds_character_at("abc", 'a', 0);
  assert_strchr_finds_character_at("abc", 'b', 1);
  assert_strchr_finds_character_at("abc", 'c', 2);
  assert_strchr_finds_character_at("abc", '\0', 3);
  assert_strchr_finds_character_at("0123456789abcdef", '0', 0);
  assert_strchr_finds_character_at("0123456789abcdef", '7', 7);
  assert_strchr_finds_character_at("0123456789abcdef", 'f', 15);

  return 0;
}
