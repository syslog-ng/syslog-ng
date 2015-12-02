/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#include "utf8utils.h"



void
assert_escaped_binary_with_unsafe_chars(const gchar *str, const gchar *expected_escaped_str, const gchar *unsafe_chars)
{
  gchar *escaped_str = convert_unsafe_utf8_to_escaped_binary(str, -1, unsafe_chars);

  assert_string(escaped_str, expected_escaped_str, "Escaped UTF-8 string is not expected");
  g_free(escaped_str);
}

void
assert_escaped_binary(const gchar *str, const gchar *expected_escaped_str)
{
  assert_escaped_binary_with_unsafe_chars(str, expected_escaped_str, NULL);
}

void
assert_escaped_text_with_unsafe_chars(const gchar *str, const gchar *expected_escaped_str, const gchar *unsafe_chars)
{
  gchar *escaped_str = convert_unsafe_utf8_to_escaped_text(str, -1, unsafe_chars);

  assert_string(escaped_str, expected_escaped_str, "Escaped UTF-8 string is not expected");
  g_free(escaped_str);
}

void
assert_escaped_text(const gchar *str, const gchar *expected_escaped_str)
{
  assert_escaped_text_with_unsafe_chars(str, expected_escaped_str, NULL);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  assert_escaped_binary("", "");
  assert_escaped_binary("\n", "\\n");
  assert_escaped_binary("\b \f \n \r \t", "\\b \\f \\n \\r \\t");
  assert_escaped_binary("\\", "\\\\");
  assert_escaped_binary("árvíztűrőtükörfúrógép", "árvíztűrőtükörfúrógép");
  assert_escaped_binary("árvíztűrőtükörfúrógép\n", "árvíztűrőtükörfúrógép\\n");
  assert_escaped_binary("\x41", "A");
  assert_escaped_binary("\x7", "\\x07");
  assert_escaped_binary("\xad", "\\xad");
  assert_escaped_binary("Á\xadÉ", "Á\\xadÉ");

  assert_escaped_binary_with_unsafe_chars("\"text\"", "\\\"text\\\"", "\"");
  assert_escaped_binary_with_unsafe_chars("\"text\"", "\\\"te\\xt\\\"", "\"x");

  assert_escaped_text("", "");
  assert_escaped_text("\n", "\\n");
  assert_escaped_text("\b \f \n \r \t", "\\b \\f \\n \\r \\t");
  assert_escaped_text("\\", "\\\\");
  assert_escaped_text("árvíztűrőtükörfúrógép", "árvíztűrőtükörfúrógép");
  assert_escaped_text("árvíztűrőtükörfúrógép\n", "árvíztűrőtükörfúrógép\\n");
  assert_escaped_text("\x41", "A");
  assert_escaped_text("\x7", "\\u0007");
  assert_escaped_text("\xad", "\\\\xad");
  assert_escaped_text("Á\xadÉ", "Á\\\\xadÉ");

  assert_escaped_text_with_unsafe_chars("\"text\"", "\\\"text\\\"", "\"");
  assert_escaped_text_with_unsafe_chars("\"text\"", "\\\"te\\xt\\\"", "\"x");

  return 0;
}
