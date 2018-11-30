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

CSVScannerOptions *
_default_options_with_flags(const gchar *columns[], gint flags)
{
  CSVScannerOptions *o;
  o = _default_options(columns);
  csv_scanner_options_set_flags(&options, flags);
  return o;
}

static gboolean
_column_name_equals(const gchar *name)
{
  return strcmp(csv_scanner_get_current_name(&scanner), name) == 0;
}

static gboolean
_column_name_unset(void)
{
  return csv_scanner_get_current_name(&scanner) == NULL;
}

static gboolean
_column_nv_equals(const gchar *name, const gchar *value)
{
  return _column_name_equals(name) && strcmp(csv_scanner_get_current_value(&scanner), value) == 0;
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
  const gchar *columns[] = { "foo", "bar", "baz", NULL };

  csv_scanner_init(&scanner, _default_options(columns), "val1,val2,val3");

  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_nv_equals("foo", "val1"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_nv_equals("bar", "val2"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_nv_equals("baz", "val3"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  cr_expect(_column_name_unset());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, empty_input_with_some_expected_columns)
{
  const gchar *columns[] = { "foo", "bar", "baz", NULL };

  csv_scanner_init(&scanner, _default_options(columns), "");

  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());
  cr_expect(!_scan_next());
  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, empty_input_with_no_columns)
{
  const gchar *columns[] = { NULL };

  csv_scanner_init(&scanner, _default_options(columns), "");

  cr_expect(_column_name_unset());
  cr_expect(!_scan_complete());
  cr_expect(!_scan_next());
  cr_expect(_column_name_unset());
  cr_expect(_scan_complete());
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, partial_input)
{
  const gchar *columns[] = { "foo", "bar", "baz", NULL };

  csv_scanner_init(&scanner, _default_options(columns), "val1,val2");

  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_nv_equals("foo", "val1"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_nv_equals("bar", "val2"));
  cr_expect(!_scan_complete());

  cr_expect(!_scan_next());
  cr_expect(_column_name_equals("baz"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(!_scan_complete());
  cr_expect(_column_name_equals("baz"));
  csv_scanner_deinit(&scanner);
}

Test(csv_scanner, greedy_column)
{
  const gchar *columns[] = { "foo", "bar", NULL };

  csv_scanner_init(&scanner, _default_options_with_flags(columns, CSV_SCANNER_GREEDY), "foo,bar,baz");

  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_name_equals("foo"));
  cr_expect(!_scan_complete());

  cr_expect(_scan_next());
  cr_expect(_column_name_equals("bar"));
  cr_expect(_column_nv_equals("bar", "bar,baz"));
  cr_expect(!_scan_complete());

  /* go past the last column */
  cr_expect(!_scan_next());
  cr_expect(_scan_complete());
  cr_expect(_column_name_unset());
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
