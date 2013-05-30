#include <stdio.h>

#include "testutils.h"
#include "str-format.h"

void
assert_str_rtrim(gchar * const string, const gchar *characters, const gchar *expected_string)
{
  gchar *rtrimmed_string = g_strdup(string);

  str_rtrim(rtrimmed_string, characters);
  assert_string(rtrimmed_string, expected_string, "Right trimmed string is not expected");
  g_free(rtrimmed_string);
}

gint
main(gint argc G_GNUC_UNUSED, gchar *argv[] G_GNUC_UNUSED)
{
  assert_str_rtrim("", "", "");
  assert_str_rtrim("apple", "", "apple");
  assert_str_rtrim("apple ", " ", "apple");
  assert_str_rtrim("apple  ", " ", "apple");
  assert_str_rtrim("apple  ", "z ", "apple");
  assert_str_rtrim("asdqweasd", "qweasd", "");

  return 0;
}
