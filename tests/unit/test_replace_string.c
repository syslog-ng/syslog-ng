#include "misc.c"
#include "testutils.h"

static void
test_replace_string(const gchar *string, const gchar *pattern, const gchar *replacement, const gchar *expected)
{
  gchar *result = replace_string(string, pattern, replacement);
  assert_string(result, expected, "Failed replacement string: %s; pattern: %s; replacement: %s", string, pattern, replacement);
  g_free(result);
}

int main(int argc, char **argv)
{
  test_replace_string("hello", "xxx", "zzz", "hello");
  test_replace_string("hello", "llo" ,"xx", "hexx");
  test_replace_string("hello",  "hel", "xx", "xxlo");
  test_replace_string("c:\\var\\c.txt", "\\", "\\\\", "c:\\\\var\\\\c.txt");
  test_replace_string("c:\\xxx\\xxx", "xxx", "var", "c:\\var\\var");
  return 0;
}
