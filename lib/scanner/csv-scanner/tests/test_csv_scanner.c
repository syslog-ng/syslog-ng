/*
 * Copyright (c) 2015-2017 Balabit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include <criterion/criterion.h>
#include "scratch-buffers.h"
#include "apphook.h"
#include "csv-scanner.h"
#include "string-list.h"

CSVScannerOptions options;

CSVScannerOptions *
_default_options(const gchar *columns[])
{
  csv_scanner_options_clean(&options);
  csv_scanner_options_set_delimiters(&options, ",");
  csv_scanner_options_set_quote_pairs(&options, "\"\"''");
  csv_scanner_options_set_flags(&options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);

  csv_scanner_options_set_columns(&options, string_array_to_list(columns));
  return &options;
}

Test(csv_scanner, simple_comma_separate_values)
{
  CSVScanner scanner;
  const gchar *columns[] = { "foo", "bar", "baz", NULL };

  csv_scanner_init(&scanner, _default_options(columns), "val1,val2,val3");

  cr_expect_str_eq(csv_scanner_get_current_name(&scanner), "foo");
  cr_assert(!csv_scanner_is_scan_complete(&scanner));

  cr_expect(csv_scanner_scan_next(&scanner));
  cr_expect_str_eq(csv_scanner_get_current_name(&scanner), "foo");
  cr_expect_str_eq(csv_scanner_get_current_value(&scanner), "val1");
  cr_expect(!csv_scanner_is_scan_complete(&scanner));

  cr_expect(csv_scanner_scan_next(&scanner));
  cr_expect_str_eq(csv_scanner_get_current_name(&scanner), "bar");
  cr_expect_str_eq(csv_scanner_get_current_value(&scanner), "val2");
  cr_expect(!csv_scanner_is_scan_complete(&scanner));

  cr_expect(csv_scanner_scan_next(&scanner));
  cr_expect_str_eq(csv_scanner_get_current_name(&scanner), "baz");
  cr_expect_str_eq(csv_scanner_get_current_value(&scanner), "val3");

  /* go past the last column */
  cr_expect(!csv_scanner_scan_next(&scanner));
  cr_expect(csv_scanner_is_scan_complete(&scanner));
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, comma_separate_values_more_columns)
{
  CSVScanner scanner;
  const gchar *columns[] = { "foo", "bar", "baz", NULL };

  csv_scanner_init(&scanner, _default_options(columns), "val1,val2,val3,val4");
  csv_scanner_scan_next(&scanner);
  csv_scanner_deinit(&scanner);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(csv_scanner, .init = setup, .fini = teardown);
