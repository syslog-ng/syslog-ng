#include "str-repr/encode.h"
#include "testutils.h"

#define str_repr_encode_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
    }                                                           \
  while (0)

#define str_repr_encode_testcase_end()                           \
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define STR_REPR_ENCODE_TESTCASE(x, ...) \
  do {                                                          \
      str_repr_encode_testcase_begin(#x, #__VA_ARGS__);  		\
      x(__VA_ARGS__);                                           \
      str_repr_encode_testcase_end();                                \
  } while(0)


GString *value;

static void
assert_encode_equals(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");

  str_repr_encode(str, input, -1);
  assert_string(str->str, expected, "Encoded value does not match expected");

  str_repr_encode(str, input, strlen(input));
  assert_string(str->str, expected, "Encoded value does not match expected");
  g_string_free(str, TRUE);
}

static void
test_encode_simple_strings(void)
{
  assert_encode_equals("", "''");
  assert_encode_equals("a", "a");
  assert_encode_equals("alma", "alma");
  assert_encode_equals("al\nma", "\"al\\nma\"");
}

static void
test_encode_strings_that_need_quotation(void)
{
  assert_encode_equals("foo bar", "\"foo bar\"");
  /* embedded quote */
  assert_encode_equals("\"value1", "'\"value1'");
  assert_encode_equals("'value1", "\"'value1\"");
  /* control sequences */
  assert_encode_equals("\b \f \n \r \t \\", "\"\\b \\f \\n \\r \\t \\\\\"");
}

int
main(int argc, char *argv[])
{
  STR_REPR_ENCODE_TESTCASE(test_encode_simple_strings);
  STR_REPR_ENCODE_TESTCASE(test_encode_strings_that_need_quotation);
  return 0;
}
