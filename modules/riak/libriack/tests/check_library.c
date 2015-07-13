#include <riack/riack.h>

START_TEST (test_riack_library_versions)
{
  ck_assert_str_eq (riack_version (), PACKAGE_VERSION);
  ck_assert_str_eq (riack_version_string (), PACKAGE_STRING);
}
END_TEST

static TCase *
test_riack_library (void)
{
  TCase *test_library;

  test_library = tcase_create ("Core");
  tcase_add_test (test_library, test_riack_library_versions);

  return test_library;
}
