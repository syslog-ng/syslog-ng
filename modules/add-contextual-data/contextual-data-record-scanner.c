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

#include "contextual-data-record-scanner.h"
#include "scanner/csv-scanner/csv-scanner.h"
#include "string-list.h"
#include "messages.h"
#include "cfg.h"

#include <string.h>

struct _ContextualDataRecordScanner
{
  ContextualDataRecord last_record;
  GlobalConfig *cfg;
  CSVScanner scanner;
  CSVScannerOptions options;
  gchar *filename;
  const gchar *name_prefix;
};

static gboolean
_fetch_next_with_prefix(ContextualDataRecordScanner *self,
                        GString *target, const gchar *prefix)
{
  if (!csv_scanner_scan_next(&self->scanner))
    {
      msg_error("add-contextual-data(): error parsing CSV file, expecting an additional column which was not found. Expecting (selector, name, value) triplets",
                evt_tag_str("filename", self->filename),
                evt_tag_str("target", csv_scanner_get_current_name(&self->scanner)));
      return FALSE;
    }
  g_string_sprintf(target, "%s%s", prefix ? prefix : "", csv_scanner_get_current_value(&self->scanner));

  return TRUE;
}

static gboolean
_fetch_next(ContextualDataRecordScanner *self, GString *target)
{
  return _fetch_next_with_prefix(self, target, NULL);
}

static gboolean
_is_whole_record_parsed(ContextualDataRecordScanner *self)
{
  if (!csv_scanner_scan_next(&self->scanner) &&
      csv_scanner_is_scan_complete(&self->scanner))
    return TRUE;

  msg_error("add-contextual-data(): extra data found at the end of line, expecting (selector, name, value) triplets",
            evt_tag_str("filename", self->filename));
  return FALSE;
}

static gboolean
_get_next_record(ContextualDataRecordScanner *self, const gchar *input, ContextualDataRecord *record)
{
  GString *buffer = g_string_sized_new(32);
  GError *error = NULL;

  csv_scanner_init(&self->scanner, &self->options, input);

  record->selector = g_string_sized_new(32);
  if (!_fetch_next(self, record->selector))
    goto error;

  if (!_fetch_next_with_prefix(self, buffer, self->name_prefix))
    goto error;

  record->value_handle = log_msg_get_value_handle(buffer->str);

  if (!_fetch_next(self, buffer))
    goto error;

  record->value = log_template_new(self->cfg, NULL);

  if (cfg_is_config_version_older(self->cfg, VERSION_VALUE_3_21) &&
      strchr(buffer->str, '$') != NULL)
    {
      msg_warning("WARNING: the value field in add-contextual-data() CSV files has been changed "
                  "to be a template starting with " VERSION_3_21 ". You are using an older config "
                  "version and your CSV file contains a '$' character in this field, which needs "
                  "to be escaped as '$$' once you change your @version declaration in the "
                  "configuration. This message means that this string is now assumed to be a "
                  "literal (non-template) string for compatibility",
                  evt_tag_str("selector", record->selector->str),
                  evt_tag_str("name", log_msg_get_value_name(record->value_handle, NULL)),
                  evt_tag_str("value", buffer->str));
      log_template_compile_literal_string(record->value, buffer->str);
    }
  else
    {
      if (!log_template_compile(record->value, buffer->str, &error))
        {
          msg_error("add-contextual-data(): error compiling template",
                   evt_tag_str("selector", record->selector->str),
                   evt_tag_str("name", log_msg_get_value_name(record->value_handle, NULL)),
                   evt_tag_str("value", buffer->str));
          goto error;
        }
    }
  g_string_free(buffer, TRUE);
  csv_scanner_deinit(&self->scanner);

  if (!_is_whole_record_parsed(self))
    goto error;

  return TRUE;
error:
  g_string_free(buffer, TRUE);
  csv_scanner_deinit(&self->scanner);
  msg_error("add-contextual-data(): the failing line is",
            evt_tag_str("filename", self->filename),
            evt_tag_str("input", input));
  return FALSE;
}

ContextualDataRecord *
contextual_data_record_scanner_get_next(ContextualDataRecordScanner *self, const gchar *input)
{
  contextual_data_record_init(&self->last_record);
  if (!_get_next_record(self, input, &self->last_record))
    {
      contextual_data_record_clean(&self->last_record);
      return NULL;
    }

  return &self->last_record;
}

void
contextual_data_record_scanner_set_name_prefix(ContextualDataRecordScanner *
                                               self, const gchar *prefix)
{
  self->name_prefix = prefix;
}

void
contextual_data_record_scanner_free(ContextualDataRecordScanner *self)
{
  csv_scanner_options_clean(&self->options);
  csv_scanner_deinit(&self->scanner);
  g_free(self->filename);
  g_free(self);
}

ContextualDataRecordScanner *
contextual_data_record_scanner_new(GlobalConfig *cfg, const gchar *filename)
{
  ContextualDataRecordScanner *self =
    g_new0(ContextualDataRecordScanner, 1);

  self->cfg = cfg;

  csv_scanner_options_set_delimiters(&self->options, ",");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  const gchar *column_array[] = { "selector", "name", "value", NULL };
  csv_scanner_options_set_columns(&self->options,
                                  string_array_to_list(column_array));
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);
  self->filename = g_strdup(filename);

  return self;
}
