#include "testutils.h"
#include "misc.h"

void
assert_utf8_escape_string_ex(const gchar *str, const gchar *expected_escaped_str)
{
  gchar *escaped_str = utf8_escape_string_ex(str);

  assert_string(escaped_str, expected_escaped_str, "Escaped UTF-8 string is not expected");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  assert_utf8_escape_string_ex("", "");
  assert_utf8_escape_string_ex("árvíztűrőtükörfúrógép", "árvíztűrőtükörfúrógép");
  assert_utf8_escape_string_ex("árvíztűrőtükörfúrógép\n", "árvíztűrőtükörfúrógép\\n");
  assert_utf8_escape_string_ex("\\", "\\\\");
  assert_utf8_escape_string_ex("\x41", "A");
  assert_utf8_escape_string_ex("\x7", "\\x07");
  assert_utf8_escape_string_ex("\b \f \n \r \t \\ \"", "\\b \\f \\n \\r \\t \\\\ \\\"");
  assert_utf8_escape_string_ex("\xad", "\\xad");
  assert_utf8_escape_string_ex("Á\xadÉ", "Á\\xadÉ");

  return 0;
}
