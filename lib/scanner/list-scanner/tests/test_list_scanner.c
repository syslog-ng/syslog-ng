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
#include "scanner/list-scanner/list-scanner.h"
#include <criterion/criterion.h>

ListScanner *list_scanner;

static void
assert_no_more_tokens(void)
{
  cr_assert(!list_scanner_scan_next(list_scanner), "list_scanner is expected to return no more key-value pairs");
}

static void
scan_next_token(void)
{
  cr_assert(list_scanner_scan_next(list_scanner),  "list_scanner is expected to return TRUE for scan_next");
}

static void
assert_current_value_is(const gchar *expected_key)
{
  const gchar *key = list_scanner_get_current_value(list_scanner);
  gint key_len = list_scanner_get_current_value_len(list_scanner);

  cr_assert_str_eq(key, expected_key, "current key mismatch");
  cr_assert_eq(key_len, strlen(expected_key));
}

static void
assert_next_value_is(const gchar *expected_value)
{
  scan_next_token();
  assert_current_value_is(expected_value);
}

static void
setup(void)
{
  list_scanner = list_scanner_new();
}

static void
teardown(void)
{
  list_scanner_free(list_scanner);
}

TestSuite(list_scanner, .init = setup, .fini = teardown);

Test(list_scanner, individual_items_are_scanned)
{
  list_scanner_input_va(list_scanner, "foo", NULL);
  assert_next_value_is("foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "foo", "bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "foo", "bar", "baz", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_next_value_is("baz");
  assert_no_more_tokens();
}

Test(list_scanner, unquoted_empty_items_are_skipped_to_make_it_easy_to_concatenate_lists)
{
  list_scanner_input_va(list_scanner, "", NULL);
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "", "foo", "bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "", "", "", ",,,,", "", "", "", "foo", "bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "foo", "", "bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "foo,", NULL);
  assert_next_value_is("foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "''", ",foo,", "bar,", ",baz", "foobar", "\"\"", NULL);
  assert_next_value_is("");
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_next_value_is("baz");
  assert_next_value_is("foobar");
  assert_next_value_is("");
  assert_no_more_tokens();
}

Test(list_scanner, quoted_empty_items_are_parsed_as_empty_values)
{
  list_scanner_input_va(list_scanner, "foo", "''", "bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("");
  assert_next_value_is("bar");
  assert_no_more_tokens();
}

Test(list_scanner, comma_delimiter_values_are_split)
{
  list_scanner_input_va(list_scanner, "foo,bar", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "foo,bar,baz", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_next_value_is("baz");
  assert_no_more_tokens();
}

Test(list_scanner, comma_and_arg_are_equivalent)
{
  list_scanner_input_va(list_scanner, "foo,bar,baz", "xxx", "", "yyy", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_next_value_is("baz");
  assert_next_value_is("xxx");
  assert_next_value_is("yyy");
  assert_no_more_tokens();
}

Test(list_scanner, works_with_gstring_input)
{
  GString *argv[] =
  {
    g_string_new("foo,bar,baz"),
    g_string_new("xxx"),
    g_string_new(""),
    g_string_new("yyy"),
  };
  const gint argc = 4;
  gint i;

  list_scanner_input_gstring_array(list_scanner, argc, argv);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_next_value_is("baz");
  assert_next_value_is("xxx");
  assert_next_value_is("yyy");
  assert_no_more_tokens();
  for (i = 0; i < argc; i++)
    g_string_free(argv[i], TRUE);
}

Test(list_scanner, handles_single_quotes)
{
  list_scanner_input_va(list_scanner, "'foo'", NULL);
  assert_next_value_is("foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "'foo','bar'", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();


  list_scanner_input_va(list_scanner, "'foo,bar'", NULL);
  assert_next_value_is("foo,bar");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "'foo''bar'", NULL);
  assert_next_value_is("'foo''bar'");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "'foo'bar", NULL);
  assert_next_value_is("'foo'bar");
  assert_no_more_tokens();
}

Test(list_scanner, handles_double_quotes)
{
  list_scanner_input_va(list_scanner, "\"foo\"", NULL);
  assert_next_value_is("foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "\"\\\"foo\"", NULL);
  assert_next_value_is("\"foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "\"foo\",\"bar\"", NULL);
  assert_next_value_is("foo");
  assert_next_value_is("bar");
  assert_no_more_tokens();


  list_scanner_input_va(list_scanner, "\"foo,bar\"", NULL);
  assert_next_value_is("foo,bar");
  assert_no_more_tokens();

  /* no separator, thus two string tokens are concatenated */
  list_scanner_input_va(list_scanner, "\"foo\"\"bar\"", NULL);
  assert_next_value_is("\"foo\"\"bar\"");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "\"foo\"bar", NULL);
  assert_next_value_is("\"foo\"bar");
  assert_no_more_tokens();
}

Test(list_scanner, malformed_quotes)
{
  list_scanner_input_va(list_scanner, "'foo", NULL);
  assert_next_value_is("'foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "bar,'foo", NULL);
  assert_next_value_is("bar");
  assert_next_value_is("'foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "bar,'foo,", NULL);
  assert_next_value_is("bar");
  assert_next_value_is("'foo,");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "\"foo", NULL);
  assert_next_value_is("\"foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "bar,\"foo", NULL);
  assert_next_value_is("bar");
  assert_next_value_is("\"foo");
  assert_no_more_tokens();

  list_scanner_input_va(list_scanner, "bar,\"foo,", NULL);
  assert_next_value_is("bar");
  assert_next_value_is("\"foo,");
  assert_no_more_tokens();
}
