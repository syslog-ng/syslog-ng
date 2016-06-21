/*
 * Copyright (c) 2015-2016 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */
#include "kv-scanner.h"
#include "testutils.h"
#include <stdarg.h>

static gboolean
_assert_no_more_tokens(KVScanner *scanner)
{
  gboolean ok = kv_scanner_scan_next(scanner);
  if (ok)
    {
      GString *msg = g_string_new("kv_scanner is expected to return no more key-value pairs ");
      do
        {
          const gchar *key = kv_scanner_get_current_key(scanner);
          if (!key)
            key = "";
          const gchar *value = kv_scanner_get_current_value(scanner);
          if (!value)
            value = "";
          g_string_append_printf(msg, "[%s/%s]", key, value);
        } while (kv_scanner_scan_next(scanner));
      expect_false(ok, msg->str);
      g_string_free(msg, TRUE);
    }
  return !ok;
}

static gboolean
_scan_next_token(KVScanner *scanner)
{
  gboolean ok = kv_scanner_scan_next(scanner);
  expect_true(ok,  "kv_scanner is expected to return TRUE for scan_next");
  return ok;
}

static gboolean
_assert_current_key_is(KVScanner *scanner, const gchar *expected_key)
{
  const gchar *key = kv_scanner_get_current_key(scanner);

  return expect_nstring(key, -1, expected_key, -1, "current key mismatch");
}

static gboolean
_assert_current_value_is(KVScanner *scanner, const gchar *expected_value)
{
  const gchar *value = kv_scanner_get_current_value(scanner);

  return expect_nstring(value, -1, expected_value, -1, "current value mismatch");
}

typedef gboolean (*ScanTestFn)(KVScanner *scanner, va_list args, gboolean *expect_more);

static gboolean
_compare_key_value(KVScanner *scanner, va_list args, gboolean *expect_more)
{
  const gchar *kv = va_arg(args, const gchar *);
  if (!kv)
    return FALSE;

  if (!_scan_next_token(scanner))
    {
      *expect_more = FALSE;
      return TRUE;
    }

  _assert_current_key_is(scanner, kv);

  kv = va_arg(args, const gchar *);
  g_assert(kv);
  _assert_current_value_is(scanner, kv);

  return TRUE;
}

static gboolean
_value_was_quoted(KVScanner *scanner, va_list args, gboolean *expect_more)
{
  if (!_compare_key_value(scanner, args, expect_more))
    return FALSE;

  gboolean was_quoted = va_arg(args, gboolean);
  g_assert((was_quoted == FALSE) ||(was_quoted == TRUE));

  expect_gboolean(scanner->value_was_quoted, was_quoted, "mismatch in value_was_quoted");
  return TRUE;
}

static void
_scan_kv_pairs_fn(KVScanner *scanner, const gchar *input, ScanTestFn fn, va_list args)
{
  g_assert(input);

  kv_scanner_input(scanner, input);
  gboolean expect_more = TRUE;
  while (fn(scanner, args, &expect_more))
    {
    }
  if (expect_more)
    _assert_no_more_tokens(scanner);
}

static void
_scan_kv_pairs_scanner(KVScanner *scanner, const gchar *input, ...)
{
  va_list args;
  va_start(args, input);

  _scan_kv_pairs_fn(scanner, input, _compare_key_value, args);

  va_end(args);
}

static void
_scan_kv_pairs_implicit(const gchar *input, ...)
{
  va_list args;
  va_start(args, input);

  KVScanner *scanner = kv_scanner_new();
  _scan_kv_pairs_fn(scanner, input, _compare_key_value, args);
  kv_scanner_free(scanner);

  va_end(args);
}

static void
_scan_kv_pairs_full(ScanTestFn fn, const gchar *input, ...)
{
  va_list args;
  va_start(args, input);

  KVScanner *scanner = kv_scanner_new();
  _scan_kv_pairs_fn(scanner, input, fn, args);
  kv_scanner_free(scanner);

  va_end(args);
}

#define TEST_KV_SCAN(input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN(%s, %s)", #input, #__VA_ARGS__); \
    _scan_kv_pairs_implicit(input, ##__VA_ARGS__, NULL); \
    testcase_end(); \
  } while (0)

#define TEST_KV_SCANNER(scanner, input, ...) \
  do { \
    testcase_begin("TEST_KV_SCANNER(%s, %s)", #input, #__VA_ARGS__); \
    _scan_kv_pairs_scanner(scanner, input, ##__VA_ARGS__, NULL); \
    testcase_end(); \
  } while (0)

#define TEST_KV_SCAN_FN(fn, input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN_FN(%s, %s, %s)", #fn, #input, #__VA_ARGS__); \
    _scan_kv_pairs_full(fn, input, ##__VA_ARGS__, NULL); \
    testcase_end(); \
  } while (0)

static void
_test_incomplete_string_returns_no_pairs(void)
{
  TEST_KV_SCAN("");
  TEST_KV_SCAN("f");
  TEST_KV_SCAN("fo");
  TEST_KV_SCAN("foo");
}

static void
_test_name_equals_value_returns_a_pair(void)
{
  TEST_KV_SCAN("foo=", "foo", "");
  TEST_KV_SCAN("foo=b", "foo", "b");
  TEST_KV_SCAN("foo=bar", "foo", "bar");
  TEST_KV_SCAN("foo=barbar", "foo", "barbar");
}

static void
_test_stray_words_are_ignored(void)
{
  TEST_KV_SCAN("lorem ipsum foo=bar", "foo", "bar");
  TEST_KV_SCAN("lorem ipsum/dolor @sitamen foo=bar", "foo", "bar");
  TEST_KV_SCAN("lorem ipsum/dolor = foo=bar\"", "foo", "bar");
  TEST_KV_SCAN("foo=bar lorem ipsum key=value some more values", "foo", "bar", "key", "value");
  TEST_KV_SCAN("*k=v", "k", "v");
  TEST_KV_SCAN("x *k=v", "k", "v");
}

static void
_test_with_multiple_key_values_return_multiple_pairs(void)
{
  TEST_KV_SCAN("key1=value1 key2=value2 key3=value3 ",
               "key1", "value1", "key2", "value2", "key3", "value3");
}

static void
_test_spaces_between_values_are_ignored(void)
{
  TEST_KV_SCAN("key1=value1    key2=value2     key3=value3 ",
               "key1", "value1", "key2", "value2", "key3", "value3");
}

static void
_test_with_comma_separated_values(void)
{
  TEST_KV_SCAN("key1=value1, key2=value2, key3=value3",
               "key1", "value1", "key2", "value2", "key3", "value3");
}

static void
_test_with_comma_separated_values_and_multiple_spaces(void)
{
  TEST_KV_SCAN("key1=value1,   key2=value2  ,    key3=value3",
               "key1", "value1",
               "key2", "value2",
               "key3", "value3");
}

static void
_test_with_comma_separated_values_without_space(void)
{
  TEST_KV_SCAN("key1=value1,key2=value2,key3=value3",
               "key1", "value1,key2=value2,key3=value3");
}

static void
_test_tab_separated_values(void)
{
  TEST_KV_SCAN("key1=value1\tkey2=value2 key3=value3",
               "key1", "value1\tkey2=value2",
               "key3", "value3");
  TEST_KV_SCAN("key1=value1,\tkey2=value2 key3=value3",
               "key1", "value1,\tkey2=value2",
               "key3", "value3");
  TEST_KV_SCAN("key1=value1\t key2=value2 key3=value3",
               "key1", "value1\t",
               "key2", "value2",
               "key3", "value3");
  TEST_KV_SCAN("k=\t",
               "k", "\t");
  TEST_KV_SCAN("k=,\t",
               "k", ",\t");
}

static void
_test_quoted_values_are_unquoted_like_c_strings(void)
{
  TEST_KV_SCAN("foo=\"bar\"", "foo", "bar");

  TEST_KV_SCAN("key1=\"value1\", key2=\"value2\"", "key1", "value1", "key2", "value2");

  /* embedded quote */
  TEST_KV_SCAN("key1=\"\\\"value1\"", "key1", "\"value1");

  /* control sequences */
  TEST_KV_SCAN("key1=\"\\b \\f \\n \\r \\t \\\\\"",
               "key1", "\b \f \n \r \t \\");

  /* unknown backslash escape is left as is */
  TEST_KV_SCAN("key1=\"\\p\"",
               "key1", "\\p");

  TEST_KV_SCAN("key1='value1', key2='value2'",
               "key1", "value1",
               "key2", "value2");

  /* embedded quote */
  TEST_KV_SCAN("key1='\\'value1'", "key1", "'value1");

  /* control sequences */
  TEST_KV_SCAN("key1='\\b \\f \\n \\r \\t \\\\'",
               "key1", "\b \f \n \r \t \\");

  /* unknown backslash escape is left as is */
  TEST_KV_SCAN("key1='\\p'", "key1", "\\p");

  TEST_KV_SCAN("key1=\\b\\f\\n\\r\\t\\\\",
               "key1", "\\b\\f\\n\\r\\t\\\\");
  TEST_KV_SCAN("key1=\b\f\n\r\\",
               "key1", "\b\f\n\r\\");
}

static void
_test_keys_without_values(void)
{
  TEST_KV_SCAN("key1 key2=value2, key3, key4=value4",
               "key2", "value2",
               "key4", "value4");

  TEST_KV_SCAN("key1= key2=value2, key3=, key4=value4 key5= , key6=value6",
               "key1", "",
               "key2", "value2",
               "key3", "",
               "key4", "value4",
               "key5", "",
               "key6", "value6");
}

static void
_test_quoted_values_with_special_characters(void)
{
  TEST_KV_SCAN("key1=\"value foo, foo2 =@,\\\"\" key2='value foo,  a='",
               "key1", "value foo, foo2 =@,\"",
               "key2", "value foo,  a=");
}

static gboolean
_parse_value_by_incrementing_all_bytes(KVScanner *self)
{
  gint i;

  g_string_assign(self->decoded_value, self->value->str);
  for (i = 0; i < self->decoded_value->len; i++)
    self->decoded_value->str[i]++;
  return TRUE;
}

static void
_test_transforms_values_if_parse_value_is_set(void)
{
  KVScanner *scanner = kv_scanner_new();
  scanner->parse_value = _parse_value_by_incrementing_all_bytes;
  TEST_KV_SCANNER(scanner, "foo=\"bar\"", "foo", "cbs");
  kv_scanner_free(scanner);
}

static void
_test_quotation_is_stored_in_the_was_quoted_value_member(void)
{
  TEST_KV_SCAN_FN(_value_was_quoted, "foo=\"bar\"", "foo", "bar", TRUE);
  TEST_KV_SCAN_FN(_value_was_quoted, "foo='bar'", "foo", "bar", TRUE);
  TEST_KV_SCAN_FN(_value_was_quoted, "foo=bar", "foo", "bar", FALSE);
  TEST_KV_SCAN_FN(_value_was_quoted, "foo='bar' foo=bar",
                  "foo", "bar", TRUE,
                  "foo", "bar", FALSE);
}

static void
_test_value_separator_with_whitespaces_around(void)
{
  KVScanner *scanner = kv_scanner_new();
  kv_scanner_set_value_separator(scanner, ':');

  TEST_KV_SCANNER(scanner, "key1: value1 key2 : value2 key3 :value3 ",
                  "key1", "");

  kv_scanner_free(scanner);
}

static void
_test_value_separator_is_used_to_separate_key_from_value(void)
{
  KVScanner *scanner = kv_scanner_new();
  kv_scanner_set_value_separator(scanner, ':');

  TEST_KV_SCANNER(scanner, "key1:value1 key2:value2 key3:value3 ",
                  "key1", "value1",
                  "key2", "value2",
                  "key3", "value3");

  kv_scanner_free(scanner);
}

static void
_test_value_separator_clone(void)
{
  KVScanner *scanner = kv_scanner_new();
  kv_scanner_set_value_separator(scanner, ':');
  KVScanner *cloned_scanner = kv_scanner_clone(scanner);
  kv_scanner_free(scanner);

  TEST_KV_SCANNER(cloned_scanner, "key1:value1 key2:value2 key3:value3 ",
                  "key1", "value1",
                  "key2", "value2",
                  "key3", "value3");
  kv_scanner_free(cloned_scanner);
}

static void
_test_invalid_encoding(void)
{
  TEST_KV_SCAN("k=\xc3", "k", "\xc3");
  TEST_KV_SCAN("k=\xc3v", "k", "\xc3v");
  TEST_KV_SCAN("k=\xff", "k", "\xff");
  TEST_KV_SCAN("k=\xffv", "k", "\xffv");
  TEST_KV_SCAN("k=\"\xc3", "k", "\xc3");
  TEST_KV_SCAN("k=\"\xc3v", "k", "\xc3v");
  TEST_KV_SCAN("k=\"\xff", "k", "\xff");
  TEST_KV_SCAN("k=\"\xffv", "k", "\xffv");
}

static void
_test_separator_in_key(void)
{
  KVScanner *scanner = kv_scanner_new();
  kv_scanner_set_value_separator(scanner, '-');

  TEST_KV_SCANNER(scanner, "k-v", "k", "v");
  TEST_KV_SCANNER(scanner, "k--v", "k", "-v");
  TEST_KV_SCANNER(scanner, "---", "-", "-");

  kv_scanner_free(scanner);
}

static void
_test_empty_keys(void)
{
  TEST_KV_SCAN("=v");
  TEST_KV_SCAN("k*=v");
  TEST_KV_SCAN("=");
  TEST_KV_SCAN("==");
  TEST_KV_SCAN("===");
  TEST_KV_SCAN(" =");
  TEST_KV_SCAN(" ==");
  TEST_KV_SCAN(" ===");
  TEST_KV_SCAN(" = =");
  TEST_KV_SCAN(" ==k=", "k", "");
  TEST_KV_SCAN(" = =k=", "k", "");
  TEST_KV_SCAN(" =k=", "k", "");
  TEST_KV_SCAN(" =k=v", "k", "v");
  TEST_KV_SCAN(" ==k=v", "k", "v");
  TEST_KV_SCAN(" =k=v=w", "k", "v=w");
}

static void
_test_unclosed_quotes(void)
{
  TEST_KV_SCAN("k=\"a", "k", "a");
  TEST_KV_SCAN("k=\\", "k", "\\");
  TEST_KV_SCAN("k=\"\\", "k", "");

  TEST_KV_SCAN("k='a", "k", "a");
  TEST_KV_SCAN("k='\\", "k", "");
}

static void
_test_comma_separator(void)
{
  TEST_KV_SCAN(", k=v", "k", "v");
  TEST_KV_SCAN(",k=v", "k", "v");
  TEST_KV_SCAN("k=v,", "k", "v,");
  TEST_KV_SCAN("k=v, ", "k", "v");
}

static void
_test_multiple_separators(void)
{
  TEST_KV_SCAN("k==", "k", "=");
  TEST_KV_SCAN("k===", "k", "==");
}

static void
_test_key_charset(void)
{
  TEST_KV_SCAN("k-j=v", "k-j", "v");
  TEST_KV_SCAN("0=v", "0", "v");
  TEST_KV_SCAN("_=v", "_", "v");
  TEST_KV_SCAN(":=v");
  TEST_KV_SCAN("Z=v", "Z", "v");
  TEST_KV_SCAN("á=v");
}

static void
_test_key_buffer_underrun(void)
{
  const gchar *buffer = "ab=v";
  const gchar *input = buffer + 2;
  TEST_KV_SCAN(input);
}

static void
test_kv_scanner(void)
{
  _test_empty_keys();
  _test_incomplete_string_returns_no_pairs();
  _test_stray_words_are_ignored();
  _test_name_equals_value_returns_a_pair();
  _test_with_multiple_key_values_return_multiple_pairs();
  _test_spaces_between_values_are_ignored();
  _test_with_comma_separated_values();
  _test_with_comma_separated_values_and_multiple_spaces();
  _test_with_comma_separated_values_without_space();
  _test_tab_separated_values();
  _test_quoted_values_are_unquoted_like_c_strings();
  _test_keys_without_values();
  _test_quoted_values_with_special_characters();
  _test_transforms_values_if_parse_value_is_set();
  _test_quotation_is_stored_in_the_was_quoted_value_member();
  _test_value_separator_is_used_to_separate_key_from_value();
  _test_value_separator_clone();
  _test_value_separator_with_whitespaces_around();
  _test_invalid_encoding();
  _test_separator_in_key();
  _test_unclosed_quotes();
  _test_comma_separator();
  _test_multiple_separators();
  _test_key_charset();
  _test_key_buffer_underrun();
}

int main(int argc, char *argv[])
{
  test_kv_scanner();
  if (testutils_deinit())
    return 0;
  else
    return 1;
}
