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


gboolean
_get_next_record(CSVTagRecordScanner *self, gchar *input, tag_record *next_record)
{
  csv_scanner_input(&self->scanner, input);
  if (!csv_scanner_scan_next(&self->scanner))
    {
      return FALSE;
    }
  next_record->selector = csv_scanner_dup_current_value(&self->scanner);
  if (!csv_scanner_scan_next(&self->scanner))
    {
      return FALSE;
    }
  next_record->name = csv_scanner_dup_current_value(&self->scanner);
  if (!csv_scanner_scan_next(&self->scanner))
    {
      return FALSE;
    }
  next_record->value = csv_scanner_dup_current_value(&self->scanner);

  return TRUE;
}

static GArray* csv_tagger_scanner_get_parsed_records(TagRecordScanner *s, FILE *file)
{
  CSVTagRecordScanner *self = (CSVTagRecordScanner *) s;
  gchar line[3072];
  tag_record next_record;
  GArray *nv_array = g_array_new(FALSE, FALSE, sizeof(tag_record));
  while(fgets(line, 3072, file))
    {
      size_t line_length = strlen(line) - 1;
      line[line_length] = '\0';
      if (_get_next_record(self, line, &next_record))
        {
          g_array_append_val(nv_array, next_record);
        }
      else
        {
          g_array_free(nv_array, TRUE);
          return NULL;
        }
    }
  return nv_array;
}

CSVTagRecordScanner*
csv_tagger_scanner_new()
{
  CSVTagRecordScanner *self = g_new0(CSVTagRecordScanner, 1);
  csv_scanner_options_set_delimiters(&self->options, ",");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  const gchar *column_array[] = {"selector", "name", "value", NULL};
  csv_scanner_options_set_columns(&self->options, string_array_to_list(column_array));
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
  csv_scanner_state_init(&self->scanner, &self->options);
  self->super.get_parsed_records = csv_tagger_scanner_get_parsed_records;
  return self;
}
