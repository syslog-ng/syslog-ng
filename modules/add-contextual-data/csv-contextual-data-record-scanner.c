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

#include "csv-contextual-data-record-scanner.h"
#include "scanner/csv-scanner/csv-scanner.h"
#include "string-list.h"
#include "messages.h"
#include "cfg.h"

#include <string.h>

static gchar *
_csv_scanner_dup_current_value_with_prefix(CSVScanner *line_scanner,
                                           const gchar *prefix)
{
  return g_strdup_printf("%s%s", prefix ? prefix : "",
                         csv_scanner_get_current_value(line_scanner));
}

static gboolean
_fetch_next_with_prefix(CSVContextualDataRecordScanner *record_scanner,
                        GString **target, const gchar *prefix)
{
  if (!csv_scanner_scan_next(&record_scanner->scanner))
    {
      msg_error("add-contextual-data(): error parsing CSV file, expecting an additional column which was not found. Expecting (selector, name, value) triplets",
                evt_tag_str("filename", record_scanner->filename),
                evt_tag_str("target", csv_scanner_get_current_name(&record_scanner->scanner)));
      return FALSE;
    }
  gchar *next = _csv_scanner_dup_current_value_with_prefix(&record_scanner->scanner, prefix);
  *target = g_string_new(next);
  g_free(next);

  return TRUE;
}

static gboolean
_fetch_next_without_prefix(CSVContextualDataRecordScanner *
                           record_scanner, GString **target)
{
  return _fetch_next_with_prefix(record_scanner, target, NULL);
}

static gboolean
_is_whole_record_parsed(CSVContextualDataRecordScanner *csv_record_scanner)
{
  if (!csv_scanner_scan_next(&csv_record_scanner->scanner) &&
      csv_scanner_is_scan_complete(&csv_record_scanner->scanner))
    return TRUE;

  msg_error("add-contextual-data(): extra data found at the end of line, expecting (selector, name, value) triplets",
            evt_tag_str("filename", csv_record_scanner->filename));
  return FALSE;
}

static gboolean
_get_next_record(ContextualDataRecordScanner *s, const gchar *input, ContextualDataRecord *record)
{
  CSVContextualDataRecordScanner *self = (CSVContextualDataRecordScanner *) s;
  GString *value_template;
  GString *name;
  GError *error = NULL;

  csv_scanner_init(&self->scanner, &self->options, input);

  if (!_fetch_next_without_prefix(self, &record->selector))
    goto error;

  if (!_fetch_next_with_prefix(self, &name, s->name_prefix))
    goto error;

  record->value_handle = log_msg_get_value_handle(name->str);
  g_string_free(name, TRUE);

  if (!_fetch_next_without_prefix(self, &value_template))
    goto error;

  record->value = log_template_new(self->super.cfg, NULL);

  if (cfg_is_config_version_older(self->super.cfg, VERSION_VALUE_3_21) &&
      strchr(value_template->str, '$') != NULL)
    {
      msg_warning("WARNING: the value field in add-contextual-data() CSV files has been changed "
                  "to be a template starting with " VERSION_3_21 ". You are using an older config "
                  "version and your CSV file contains a '$' character in this field, which needs "
                  "to be escaped as '$$' once you change your @version declaration in the "
                  "configuration. This message means that this string is now assumed to be a "
                  "literal (non-template) string for compatibility",
                  evt_tag_str("selector", record->selector->str),
                  evt_tag_str("name", log_msg_get_value_name(record->value_handle, NULL)),
                  evt_tag_str("value", value_template->str));
      log_template_compile_literal_string(record->value, value_template->str);
    }
  else
    {
      if (!log_template_compile(record->value, value_template->str, &error))
        {
          msg_error("add-contextual-data(): error compiling template",
                   evt_tag_str("selector", record->selector->str),
                   evt_tag_str("name", log_msg_get_value_name(record->value_handle, NULL)),
                   evt_tag_str("value", value_template->str));
          g_string_free(value_template, TRUE);
          goto error;
        }
    }
  g_string_free(value_template, TRUE);

  if (!_is_whole_record_parsed(self))
    goto error;

  return TRUE;
error:
  msg_error("add-contextual-data(): the failing line is",
            evt_tag_str("filename", self->filename),
            evt_tag_str("input", input));
  return FALSE;
}

static void
csv_contextual_data_record_scanner_free(ContextualDataRecordScanner *s)
{
  CSVContextualDataRecordScanner *self = (CSVContextualDataRecordScanner *) s;
  csv_scanner_options_clean(&self->options);
  csv_scanner_deinit(&self->scanner);
  g_free(self->filename);
  g_free(self);
}

ContextualDataRecordScanner *
csv_contextual_data_record_scanner_new(GlobalConfig *cfg, const gchar *filename)
{
  CSVContextualDataRecordScanner *self =
    g_new0(CSVContextualDataRecordScanner, 1);

  contextual_data_record_scanner_init(&self->super, cfg);
  self->super.get_next = _get_next_record;
  self->super.free_fn = csv_contextual_data_record_scanner_free;

  csv_scanner_options_set_delimiters(&self->options, ",");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  const gchar *column_array[] = { "selector", "name", "value", NULL };
  csv_scanner_options_set_columns(&self->options,
                                  string_array_to_list(column_array));
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);
  self->filename = g_strdup(filename);

  return &self->super;
}
