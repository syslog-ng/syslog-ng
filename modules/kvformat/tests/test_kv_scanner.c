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
        }
      while (kv_scanner_scan_next(scanner));
      expect_false(ok, msg->str);
      g_string_free(msg, TRUE);
    }
  return !ok;
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

static gboolean
_compare_key_value(KVScanner *scanner, const gchar *key, const gchar *value, gboolean *expect_more)
{
  g_assert(value);

  gboolean ok = kv_scanner_scan_next(scanner);
  if (ok)
    {
      _assert_current_key_is(scanner, key);
      _assert_current_value_is(scanner, value);
      return TRUE;
    }
  else
    {
      *expect_more = FALSE;
      expect_true(ok, "kv_scanner is expected to return TRUE for scan_next(), "
                  "first unconsumed pair: [%s/%s]",
                  key, value);
      return FALSE;
    }
}

typedef const gchar *const VAElement;

static void
_scan_kv_pairs_scanner(KVScanner *scanner, const gchar *input, VAElement args[])
{
  g_assert(input);
  kv_scanner_input(scanner, input);
  gboolean expect_more = TRUE;
  const gchar *key = *(args++);
  while (key)
    {
      const gchar *value = *(args++);
      if (!value || !_compare_key_value(scanner, key, value, &expect_more))
        break;
      key = *(args++);
    }
  if (expect_more)
    _assert_no_more_tokens(scanner);
}

static void
_scan_kv_pairs_implicit(const gchar *input, VAElement args[])
{
  KVScanner *scanner = kv_scanner_new();
  _scan_kv_pairs_scanner(scanner, input, args);
  kv_scanner_free(scanner);
}

typedef struct _KVQElement
{
  const gchar *const key;
  const gchar *const value;
  gboolean quoted;
} KVQElement;

typedef struct _KVQContainer
{
  const gsize n;
  const KVQElement *const arg;
} KVQContainer;

#define VARARG_STRUCT(VARARG_STRUCT_cont, VARARG_STRUCT_elem, ...) \
  (const VARARG_STRUCT_cont) { \
    sizeof((const VARARG_STRUCT_elem[]) { __VA_ARGS__ }) / sizeof(VARARG_STRUCT_elem), \
    (const VARARG_STRUCT_elem[]){__VA_ARGS__}\
  }

static void
_scan_kv_pairs_quoted(const gchar *input, KVQContainer args)
{
  KVScanner *scanner = kv_scanner_new();

  g_assert(input);
  kv_scanner_input(scanner, input);
  gboolean expect_more = TRUE;
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_compare_key_value(scanner, args.arg[i].key, args.arg[i].value, &expect_more))
        break;
      expect_gboolean(scanner->value_was_quoted, args.arg[i].quoted,
                      "mismatch in value_was_quoted for [%s/%s]",
                      args.arg[i].key, args.arg[i].value);
    }
  if (expect_more)
    _assert_no_more_tokens(scanner);
  kv_scanner_free(scanner);
}

#define TEST_KV_SCAN(TEST_KV_SCAN_input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN(%s, %s)", #TEST_KV_SCAN_input, #__VA_ARGS__); \
    _scan_kv_pairs_implicit(TEST_KV_SCAN_input, (VAElement[]){ __VA_ARGS__,  NULL }); \
    testcase_end(); \
  } while (0)

#define TEST_KV_SCAN0(TEST_KV_SCAN_input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN(%s, %s)", #TEST_KV_SCAN_input, #__VA_ARGS__); \
    _scan_kv_pairs_implicit(TEST_KV_SCAN_input, (VAElement[]){ NULL }); \
    testcase_end(); \
  } while (0)

#define TEST_KV_SCANNER(TEST_KV_SCAN_scanner, TEST_KV_SCAN_input, ...) \
  do { \
    testcase_begin("TEST_KV_SCANNER(%s, %s)", #TEST_KV_SCAN_input, #__VA_ARGS__); \
    _scan_kv_pairs_scanner(TEST_KV_SCAN_scanner, TEST_KV_SCAN_input, \
      (VAElement[]){ __VA_ARGS__,  NULL }); \
    testcase_end(); \
  } while (0)

#define TEST_KV_SCAN_Q(TEST_KV_SCAN_input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN_Q(%s, %s)", #TEST_KV_SCAN_input, #__VA_ARGS__); \
    _scan_kv_pairs_quoted(TEST_KV_SCAN_input, VARARG_STRUCT(KVQContainer, KVQElement, __VA_ARGS__)); \
    testcase_end(); \
  } while (0)

static void
_test_incomplete_string_returns_no_pairs(void)
{
  TEST_KV_SCAN0("");
  TEST_KV_SCAN0("f");
  TEST_KV_SCAN0("fo");
  TEST_KV_SCAN0("foo");
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
  TEST_KV_SCAN_Q("foo=\"bar\"", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q("foo='bar'", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q("foo=bar", {"foo", "bar", FALSE});
  TEST_KV_SCAN_Q("foo='bar' k=v",
  {"foo", "bar", TRUE},
  {"k", "v", FALSE});
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
  TEST_KV_SCAN0("=v");
  TEST_KV_SCAN0("k*=v");
  TEST_KV_SCAN0("=");
  TEST_KV_SCAN0("==");
  TEST_KV_SCAN0("===");
  TEST_KV_SCAN0(" =");
  TEST_KV_SCAN0(" ==");
  TEST_KV_SCAN0(" ===");
  TEST_KV_SCAN0(" = =");
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
  TEST_KV_SCAN0(":=v");
  TEST_KV_SCAN("Z=v", "Z", "v");
  TEST_KV_SCAN0("á=v");
}

static void
_test_key_buffer_underrun(void)
{
  const gchar *buffer = "ab=v";
  const gchar *input = buffer + 2;
  TEST_KV_SCAN0(input);
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
