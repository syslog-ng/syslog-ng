#include "kv-scanner.h"
#include "testutils.h"

#define kv_scanner_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      kv_scanner_init(&kv_scanner);                             \
    }                                                           \
  while (0)

#define kv_scanner_testcase_end()                           \
  do                                                            \
    {                                                           \
      kv_scanner_destroy(&kv_scanner);                          \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define KV_SCANNER_TESTCASE(x, ...) \
  do {                                                          \
      kv_scanner_testcase_begin(#x, #__VA_ARGS__);  		\
      x(__VA_ARGS__);                                           \
      kv_scanner_testcase_end();                                \
  } while(0)

KVScannerState kv_scanner;

static void
assert_no_more_tokens(void)
{
  assert_false(kv_scanner_scan_next(&kv_scanner), "kv_scanner is expected to return no more key-value pairs");
}

static void
scan_next_token(void)
{
  assert_true(kv_scanner_scan_next(&kv_scanner),  "kv_scanner is expected to return TRUE for scan_next");
}

static void
assert_current_key_is(const gchar *expected_key)
{
  const gchar *key = kv_scanner_get_current_key(&kv_scanner);

  assert_string(key, expected_key, "current key mismatch");
}

static void
assert_current_value_is(const gchar *expected_value)
{
  const gchar *value = kv_scanner_get_current_value(&kv_scanner);

  assert_string(value, expected_value, "current value mismatch");
}

static void
assert_current_kv_is(const gchar *expected_key, const gchar *expected_value)
{
  assert_current_key_is(expected_key);
  assert_current_value_is(expected_value);
}

static void
assert_next_kv_is(const gchar *expected_key, const gchar *expected_value)
{
  scan_next_token();
  assert_current_kv_is(expected_key, expected_value);
}

static void
test_kv_scanner_incomplete_string_returns_no_pairs(void)
{
  kv_scanner_input(&kv_scanner, "");
  assert_no_more_tokens();
  kv_scanner_input(&kv_scanner, "f");
  assert_no_more_tokens();
  kv_scanner_input(&kv_scanner, "fo");
  assert_no_more_tokens();
  kv_scanner_input(&kv_scanner, "foo");
  assert_no_more_tokens();
}

static void
test_kv_scanner_name_equals_value_returns_a_pair(void)
{
  kv_scanner_input(&kv_scanner, "foo=");
  assert_next_kv_is("foo", "");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "foo=b");
  assert_next_kv_is("foo", "b");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "foo=bar");
  assert_next_kv_is("foo", "bar");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "foo=barbar ");
  assert_next_kv_is("foo", "barbar");
  assert_no_more_tokens();
}

static void
test_kv_scanner_stray_words_are_ignored(void)
{
  kv_scanner_input(&kv_scanner, "lorem ipsum foo=bar");
  assert_next_kv_is("foo", "bar");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "foo=bar lorem ipsum key=value some more values");
  assert_next_kv_is("foo", "bar");
  assert_next_kv_is("key", "value");
  assert_no_more_tokens();
}

static void
test_kv_scanner_with_multiple_key_values_return_multiple_pairs(void)
{
  kv_scanner_input(&kv_scanner, "key1=value1 key2=value2 key3=value3 ");
  assert_next_kv_is("key1", "value1");
  assert_next_kv_is("key2", "value2");
  assert_next_kv_is("key3", "value3");
  assert_no_more_tokens();
}

static void
test_kv_scanner_spaces_between_values_are_ignored(void)
{
  kv_scanner_input(&kv_scanner, "key1=value1    key2=value2     key3=value3 ");
  assert_next_kv_is("key1", "value1");
  assert_next_kv_is("key2", "value2");
  assert_next_kv_is("key3", "value3");
  assert_no_more_tokens();
}

static void
test_kv_scanner_with_comma_separated_values(void)
{
  kv_scanner_input(&kv_scanner, "key1=value1, key2=value2, key3=value3");
  assert_next_kv_is("key1", "value1");
  assert_next_kv_is("key2", "value2");
  assert_next_kv_is("key3", "value3");
  assert_no_more_tokens();
}

static void
test_kv_scanner_quoted_values_are_unquoted_like_c_strings(void)
{
  kv_scanner_input(&kv_scanner, "foo=\"bar\"");
  assert_next_kv_is("foo", "bar");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "key1=\"value1\", key2=\"value2\"");
  assert_next_kv_is("key1", "value1");
  assert_next_kv_is("key2", "value2");
  assert_no_more_tokens();

  /* embedded quote */
  kv_scanner_input(&kv_scanner, "key1=\"\\\"value1\"");
  assert_next_kv_is("key1", "\"value1");
  assert_no_more_tokens();

  /* control sequences */
  kv_scanner_input(&kv_scanner, "key1=\"\\b \\f \\n \\r \\t \\\\\"");
  assert_next_kv_is("key1", "\b \f \n \r \t \\");
  assert_no_more_tokens();

  /* unknown backslash escape is left as is */
  kv_scanner_input(&kv_scanner, "key1=\"\\p\"");
  assert_next_kv_is("key1", "\\p");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "key1='value1', key2='value2'");
  assert_next_kv_is("key1", "value1");
  assert_next_kv_is("key2", "value2");
  assert_no_more_tokens();

  /* embedded quote */
  kv_scanner_input(&kv_scanner, "key1='\\'value1'");
  assert_next_kv_is("key1", "'value1");
  assert_no_more_tokens();

  /* control sequences */
  kv_scanner_input(&kv_scanner, "key1='\\b \\f \\n \\r \\t \\\\'");
  assert_next_kv_is("key1", "\b \f \n \r \t \\");
  assert_no_more_tokens();

  /* unknown backslash escape is left as is */
  kv_scanner_input(&kv_scanner, "key1='\\p'");
  assert_next_kv_is("key1", "\\p");
  assert_no_more_tokens();
}

static void
test_kv_scanner(void)
{
  KV_SCANNER_TESTCASE(test_kv_scanner_incomplete_string_returns_no_pairs);
  KV_SCANNER_TESTCASE(test_kv_scanner_stray_words_are_ignored);
  KV_SCANNER_TESTCASE(test_kv_scanner_name_equals_value_returns_a_pair);
  KV_SCANNER_TESTCASE(test_kv_scanner_with_multiple_key_values_return_multiple_pairs);
  KV_SCANNER_TESTCASE(test_kv_scanner_spaces_between_values_are_ignored);
  KV_SCANNER_TESTCASE(test_kv_scanner_with_comma_separated_values);
  KV_SCANNER_TESTCASE(test_kv_scanner_quoted_values_are_unquoted_like_c_strings);
}

int main(int argc, char *argv[])
{
  test_kv_scanner();
}
