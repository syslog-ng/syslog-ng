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
CSVScanner scanner;

CSVScannerOptions *
_default_options(gint expected_columns)
{
  csv_scanner_options_clean(&options);
  csv_scanner_options_set_delimiters(&options, ",");
  csv_scanner_options_set_quote_pairs(&options, "\"\"''");
  csv_scanner_options_set_flags(&options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);

  csv_scanner_options_set_expected_columns(&options, expected_columns);
  return &options;
}

CSVScannerOptions *
_default_options_with_flags(gint expected_columns, gint flags)
{
  CSVScannerOptions *o;
  o = _default_options(expected_columns);
  csv_scanner_options_set_flags(&options, flags);
  return o;
}

static gboolean
_column_index_equals(gint column)
{
  return csv_scanner_get_current_column(&scanner) == column;
}

static gboolean
_column_equals(gint column, const gchar *value)
{
  return _column_index_equals(column) && strcmp(csv_scanner_get_current_value(&scanner), value) == 0;
}

static gboolean
_scan_complete(void)
{
  return csv_scanner_is_scan_complete(&scanner);
}

static gboolean
_scan_next(void)
{
  return csv_scanner_scan_next(&scanner);
}

Test(csv_scanner, simple_comma_separate_values)
{
  csv_scanner_init(&scanner, _default_options(3), "val1,val2,val3");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(0, "val1"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(1, "val2"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(2, "val3"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, null_value)
{
  _default_options(3);
  csv_scanner_options_set_null_value(&options, "null");
  csv_scanner_init(&scanner, &options, "val1,null,val3");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(0, "val1"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(1, ""));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(2, "val3"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, empty_input_with_some_expected_columns)
{
  csv_scanner_init(&scanner, _default_options(3), "");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());
  cr_expect(!_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, empty_input_with_no_columns)
{
  csv_scanner_init(&scanner, _default_options(0), "");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());
  cr_expect(!_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, partial_input)
{
  csv_scanner_init(&scanner, _default_options(3), "val1,val2");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(0, "val1"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_equals(1, "val2"));
  cr_expect(!_scan_complete());

  cr_expect(!_scan_next());
  cr_expect(_column_index_equals(2));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(!_scan_complete());
  cr_expect(_column_index_equals(2));
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, greedy_column)
{
  csv_scanner_init(&scanner, _default_options_with_flags(2, CSV_SCANNER_GREEDY), "foo,bar,baz");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "bar,baz"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, greedy_column_strip_whitespace)
{
  csv_scanner_init(&scanner, _default_options_with_flags(2, CSV_SCANNER_GREEDY|CSV_SCANNER_STRIP_WHITESPACE),
                   "foo,  bar,baz  ");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "bar,baz"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, greedy_column_null_value)
{
  _default_options_with_flags(2, CSV_SCANNER_GREEDY|CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_null_value(&options, "bar,baz");
  csv_scanner_init(&scanner, &options, "foo,  bar,baz  ");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, ""));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, escape_double_char)
{
  _default_options_with_flags(2, CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);
  csv_scanner_init(&scanner, &options, "foo,\"this is a single quote \"\" character\"");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "this is a single quote \" character"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, escape_backslash)
{
  _default_options_with_flags(2, CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_BACKSLASH);
  csv_scanner_init(&scanner, &options, "foo,\"this is a single quote \\\" character\\n\"");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "this is a single quote \" charactern"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, escape_backslash_sequences)
{
  _default_options_with_flags(2, CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES);
  csv_scanner_init(&scanner, &options, "foo,\"\\\"\\a\\t\\v\\r\\n\\\"\"");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "\"\a\t\v\r\n\""));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, escape_backslash_x_sequences)
{
  _default_options_with_flags(2, CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES);
  csv_scanner_init(&scanner, &options, "foo,\"\\x41\\x00\\x40\"");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "A\x00@"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, escape_backslash_invalid_x_sequence)
{
  _default_options_with_flags(2, CSV_SCANNER_STRIP_WHITESPACE);

  csv_scanner_options_set_dialect(&options, CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES);
  csv_scanner_init(&scanner, &options, "foo,\"\\x4Q\"");

  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(0));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_index_equals(1));
  cr_expect(_column_equals(1, "x4Q"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, columnless_no_flags)
{
  csv_scanner_init(&scanner, _default_options(0), "val1,val2,val3");

  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(strcmp(csv_scanner_get_current_value(&scanner), "val1")==0);
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(strcmp(csv_scanner_get_current_value(&scanner), "val2")==0);
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(strcmp(csv_scanner_get_current_value(&scanner), "val3")==0);
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
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
