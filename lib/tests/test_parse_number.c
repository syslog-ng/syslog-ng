#include <stdlib.h>

#include "testutils.h"
#include "parse-number.h"

static void
assert_parse_with_suffix(const gchar *str, gint64 expected)
{
  gint64 n;
  gboolean res;

  res = parse_number_with_suffix(str, &n);

  assert_gboolean(res, TRUE, "Parsing (w/ suffix) %s failed", str);
  assert_gint64(n, expected, "Parsing (w/ suffix) %s failed", str);
}

static void
assert_parse_with_suffix_fails(const gchar *str)
{
  gint64 n;
  gboolean res;

  res = parse_number_with_suffix(str, &n);
  assert_gboolean(res, FALSE, "Parsing (w/ suffix) %s succeeded, while expecting failure", str);
}

static void
assert_parse(const gchar *str, gint64 expected)
{
  gint64 n;
  gboolean res;

  res = parse_number(str, &n);

  assert_gboolean(res, TRUE, "Parsing (w/o suffix) %s failed", str);
  assert_gint64(n, expected, "Parsing (w/o suffix) %s failed", str);
}

static void
assert_parse_fails(const gchar *str)
{
  gint64 n;
  gboolean res;

  res = parse_number(str, &n);

  assert_gboolean(res, FALSE, "Parsing (w/o suffix) %s succeeded, while expecting failure", str);
}

static void
test_simple_numbers_are_parsed_properly(void)
{
  assert_parse_with_suffix("1234", 1234);
  assert_parse_with_suffix("+1234", 1234);
  assert_parse_with_suffix("-1234", -1234);

  assert_parse("1234", 1234);
}

static void
test_exponent_suffix_is_parsed_properly(void)
{
  assert_parse_with_suffix("1K", 1000);
  assert_parse_with_suffix("1k", 1000);
  assert_parse_with_suffix("1m", 1000 * 1000);
  assert_parse_with_suffix("1M", 1000 * 1000);
  assert_parse_with_suffix("1G", 1000 * 1000 * 1000);
  assert_parse_with_suffix("1g", 1000 * 1000 * 1000);
}

static void
test_byte_units_are_accepted(void)
{
  assert_parse_with_suffix("1b", 1);
  assert_parse_with_suffix("1B", 1);
  assert_parse_with_suffix("1Kb", 1000);
  assert_parse_with_suffix("1kB", 1000);
  assert_parse_with_suffix("1mb", 1000 * 1000);
  assert_parse_with_suffix("1MB", 1000 * 1000);
  assert_parse_with_suffix("1Gb", 1000 * 1000 * 1000);
  assert_parse_with_suffix("1gB", 1000 * 1000 * 1000);
}

static void
test_base2_is_selected_by_an_i_modifier(void)
{
  assert_parse_with_suffix("1Kib", 1024);
  assert_parse_with_suffix("1kiB", 1024);
  assert_parse_with_suffix("1Ki", 1024);
  assert_parse_with_suffix("1kI", 1024);
  assert_parse_with_suffix("1mib", 1024 * 1024);
  assert_parse_with_suffix("1MiB", 1024 * 1024);
  assert_parse_with_suffix("1Gib", 1024 * 1024 * 1024);
  assert_parse_with_suffix("1giB", 1024 * 1024 * 1024);
  assert_parse_with_suffix("1024giB", 1024LL * 1024LL * 1024LL * 1024LL);
}

static void
test_invalid_formats_are_not_accepted(void)
{
  assert_parse_with_suffix_fails("1234Z");
  assert_parse_with_suffix_fails("1234kZ");
  assert_parse_with_suffix_fails("1234kdZ");
  assert_parse_with_suffix_fails("1234kiZ");
  assert_parse_fails("1234kiZ");
}

int
main(int argc, char *argv[])
{
  test_simple_numbers_are_parsed_properly();
  test_exponent_suffix_is_parsed_properly();
  test_byte_units_are_accepted();
  test_base2_is_selected_by_an_i_modifier();
  test_invalid_formats_are_not_accepted();

  return 0;
}
