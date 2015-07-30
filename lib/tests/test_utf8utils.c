#include "testutils.h"
#include "utf8utils.h"



void
assert_escaped_binary_with_unsafe_chars(const gchar *str, const gchar *expected_escaped_str, const gchar *unsafe_chars)
{
  gchar *escaped_str = convert_unsafe_utf8_to_escaped_binary(str, unsafe_chars);

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
  gchar *escaped_str = convert_unsafe_utf8_to_escaped_text(str, unsafe_chars);

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
