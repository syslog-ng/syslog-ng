/*
 * Copyright (c) 2016 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "csv-tagrecord-scanner.h"
#include "scanner/csv-scanner/csv-scanner.h"
#include "string-list.h"
#include <string.h>


static gchar*
_csv_scanner_dup_current_value_with_prefix(CSVScanner *line_scanner, const gchar *prefix)
{
  if (!prefix)
    return csv_scanner_dup_current_value(line_scanner);

  return g_strdup_printf("%s.%s", prefix, csv_scanner_get_current_value(line_scanner));
}

static gboolean
_store_next_record_with_prefix(TagRecordScanner *record_scanner, gchar **lvalue, const gchar *prefix)
{
  CSVScanner *line_scanner = (CSVScanner *) record_scanner->scanner;
  if (!csv_scanner_scan_next(line_scanner))
    {
      tag_record_clean(&record_scanner->last_record);
      return FALSE;
    }

  *lvalue = _csv_scanner_dup_current_value_with_prefix(line_scanner, prefix);

  return TRUE;
}

static gboolean
_store_next_record_without_prefix(TagRecordScanner *record_scanner, gchar **lvalue)
{
  return _store_next_record_with_prefix(record_scanner, lvalue, NULL);
}

static gboolean
_get_next_record(TagRecordScanner *s, const gchar *input, TagRecord *next_record)
{
  CSVScanner *line_scanner = (CSVScanner *) s->scanner;
  csv_scanner_input(line_scanner, input);

  if (!_store_next_record_without_prefix(s, &next_record->selector))
    return FALSE;

  if (!_store_next_record_with_prefix(s, &next_record->name, s->name_prefix))
    return FALSE;

  if (!_store_next_record_without_prefix(s, &next_record->value))
    return FALSE;

  return TRUE;
}

const TagRecord*
get_next_record(TagRecordScanner *self, const gchar *input)
{
  if (!_get_next_record(self, input, &self->last_record))
    return NULL;

  return &self->last_record;
}

static void
csv_tag_record_scanner_free(TagRecordScanner *s)
{
  CSVTagRecordScanner *self = (CSVTagRecordScanner *)s;
  csv_scanner_options_clean(&self->options);
  csv_scanner_state_clean(&self->scanner);
  g_free(self);
}

TagRecordScanner*
csv_tag_record_scanner_new()
{
  CSVTagRecordScanner *self = g_new0(CSVTagRecordScanner, 1);
  csv_scanner_options_set_delimiters(&self->options, ",");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  const gchar *column_array[] = {"selector", "name", "value", NULL};
  csv_scanner_options_set_columns(&self->options, string_array_to_list(column_array));
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
  csv_scanner_state_init(&self->scanner, &self->options);
  self->super.scanner = &self->scanner;
  self->super.get_next = get_next_record;
  self->super.free_fn = csv_tag_record_scanner_free;
  return &self->super;
}
