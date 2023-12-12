/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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

#include "csvparser.h"
#include "scanner/csv-scanner/csv-scanner.h"
#include "parser/parser-expr.h"
#include "scratch-buffers.h"

#include <string.h>


CSVParserColumn *
csv_parser_column_new(const gchar *name, LogMessageValueType type)
{
  CSVParserColumn *self = g_new0(CSVParserColumn, 1);

  self->name = g_strdup(name);
  self->type = type;
  return self;
}

CSVParserColumn *
csv_parser_column_clone(CSVParserColumn *other)
{
  return csv_parser_column_new(other->name, other->type);
}

void
csv_parser_column_free(CSVParserColumn *self)
{
  g_free(self->name);
  g_free(self);
}

typedef struct _CSVParser
{
  LogParser super;
  CSVScannerOptions options;
  GList *columns;
  gchar *prefix;
  gint prefix_len;
  gint on_error;
} CSVParser;

#define CSV_PARSER_FLAGS_SHIFT 16
#define CSV_PARSER_FLAGS_MASK  0xFFFF0000
#define CSV_SCANNER_FLAGS_MASK 0xFFFF

#define CSV_PARSER_DIALECT_MASK              (0x7 << CSV_PARSER_FLAGS_SHIFT)
#define CSV_PARSER_ESCAPE_MODE_NONE          (1 << CSV_PARSER_FLAGS_SHIFT)
#define CSV_PARSER_ESCAPE_MODE_BACKSLASH     (2 << CSV_PARSER_FLAGS_SHIFT)
#define CSV_PARSER_ESCAPE_MODE_DOUBLE_CHAR   (4 << CSV_PARSER_FLAGS_SHIFT)
#define CSV_PARSER_DROP_INVALID              (8 << CSV_PARSER_FLAGS_SHIFT)

CSVScannerOptions *
csv_parser_get_scanner_options(LogParser *s)
{
  CSVParser *self = (CSVParser *) s;

  return &self->options;
}

void
csv_parser_set_columns(LogParser *s, GList *columns)
{
  CSVParser *self = (CSVParser *) s;

  self->columns = columns;
}

gboolean
csv_parser_set_flags(LogParser *s, guint32 flags)
{
  CSVParser *self = (CSVParser *) s;
  guint32 dialect = (flags & CSV_PARSER_DIALECT_MASK);
  guint32 scanner_flags = flags & CSV_SCANNER_FLAGS_MASK;

  csv_scanner_options_set_flags(&self->options, scanner_flags);
  switch (dialect)
    {
    case 0:
      break;
    case CSV_PARSER_ESCAPE_MODE_NONE:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
      break;
    case CSV_PARSER_ESCAPE_MODE_BACKSLASH:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_BACKSLASH);
      break;
    case CSV_PARSER_ESCAPE_MODE_DOUBLE_CHAR:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);
      break;
    default:
      return FALSE;
    }
  if (flags & CSV_PARSER_DROP_INVALID)
    csv_parser_set_drop_invalid((LogParser *) self, TRUE);
  return TRUE;
}

void
csv_parser_set_prefix(LogParser *s, const gchar *prefix)
{
  CSVParser *self = (CSVParser *) s;

  g_free(self->prefix);
  if (prefix)
    {
      self->prefix = g_strdup(prefix);
      self->prefix_len = strlen(prefix);
    }
  else
    {
      self->prefix = NULL;
      self->prefix_len = 0;
    }
}

void
csv_parser_set_drop_invalid(LogParser *s, gboolean drop_invalid)
{
  CSVParser *self = (CSVParser *) s;
  if (drop_invalid)
    self->on_error |= ON_ERROR_DROP_MESSAGE;
  else
    self->on_error &= ~ON_ERROR_DROP_MESSAGE;
}

static const gchar *
_format_key_for_prefix(GString *scratch, const gchar *key, const gint prefix_len)
{
  g_string_truncate(scratch, prefix_len);
  g_string_append(scratch, key);
  return scratch->str;
}

static const gchar *
_return_key(GString *scratch, const gchar *key, const gint prefix_len)
{
  return key;
}

typedef const gchar *(*key_formatter_t)(GString *scratch, const gchar *key, const gint prefix_len);

static key_formatter_t
dispatch_key_formatter(gchar *prefix)
{
  return prefix ? _format_key_for_prefix : _return_key;
}

gboolean
_should_drop_message(CSVParser *self)
{
  return (self->on_error & ON_ERROR_DROP_MESSAGE);
}

static gboolean
_process_column_on_invalid_type_cast(CSVParser *self, LogMessageValueType *current_type)
{
  if(self->on_error & ON_ERROR_FALLBACK_TO_STRING)
    {
      if (!(self->on_error & ON_ERROR_SILENT))
        {
          msg_debug("csv-parser: error casting value to the type specified, trying string type because on-error(\"fallback-to-string\") was specified");
        }
      *current_type = LM_VT_STRING;
      return TRUE;
    }
  else if (self->on_error & ON_ERROR_DROP_PROPERTY)
    {
      if (!(self->on_error & ON_ERROR_SILENT))
        {
          msg_debug("csv-parser: error casting value to the type specified, dropping property because on-error(\"drop-property\") was specified");
        }
    }
  return FALSE;
}

static gboolean
_process_column(CSVParser *self, CSVScanner *scanner, LogMessage *msg, CSVParserColumn *current_column,
                GString *key_scratch)
{

  LogMessageValueType current_column_type = current_column->type;
  const gchar *current_value = csv_scanner_get_current_value(scanner);
  GError *error = NULL;
  key_formatter_t _key_formatter = dispatch_key_formatter(self->prefix);
  gboolean should_set_value = TRUE;

  if (!type_cast_validate(current_value, -1, current_column_type, &error))
    {
      if (!(self->on_error & ON_ERROR_SILENT))
        {
          msg_debug("csv-parser: error casting value to the type specified",
                    evt_tag_str("error", error->message)
                   );
        }
      g_clear_error(&error);

      gboolean need_drop = type_cast_drop_helper(self->on_error, current_value, -1,
                                                 log_msg_value_type_to_str(current_column_type));
      if (need_drop)
        {
          return FALSE;
        }

      should_set_value = _process_column_on_invalid_type_cast(self, &current_column_type);
    }

  if (should_set_value)
    {
      log_msg_set_value_by_name_with_type(msg,
                                          _key_formatter(key_scratch, current_column->name, self->prefix_len),
                                          csv_scanner_get_current_value(scanner),
                                          csv_scanner_get_current_value_len(scanner),
                                          current_column_type);
    }
  return TRUE;

}

static gboolean
_iterate_columns(CSVParser *self, CSVScanner *scanner, LogMessage *msg)
{
  GString *key_scratch = scratch_buffers_alloc();
  GList *column_l = self->columns;
  if (self->prefix)
    g_string_assign(key_scratch, self->prefix);

  gint match_index = 1;

  while (csv_scanner_scan_next(scanner))
    {
      if (self->columns)
        {
          CSVParserColumn *current_column = column_l->data;
          if (!_process_column(self, scanner, msg, current_column, key_scratch))
            {
              return FALSE;
            }
          column_l = g_list_next(column_l);
        }
      else
        {
          if (match_index == 1)
            log_msg_unset_match(msg, 0);
          log_msg_set_match_with_type(msg,
                                      match_index, csv_scanner_get_current_value(scanner),
                                      csv_scanner_get_current_value_len(scanner),
                                      LM_VT_STRING);
          match_index++;
        }
    }
  return TRUE;
}

static gboolean
csv_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                   gsize input_len)
{
  CSVParser *self = (CSVParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);

  msg_trace("csv-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_str ("prefix", self->prefix),
            evt_tag_msg_reference(msg));
  CSVScanner scanner;
  csv_scanner_init(&scanner, &self->options, input);

  gboolean result = TRUE;

  if (!_iterate_columns(self, &scanner, msg))
    result = FALSE;
  if (!csv_scanner_is_scan_complete(&scanner))
    result = FALSE;

  if (_should_drop_message(self) && !result && !(self->on_error & ON_ERROR_SILENT))
    {
      msg_debug("csv-parser() failed",
                evt_tag_str("error",
                            "csv-parser() failed to parse its input and drop-invalid(yes) or on-error(\"drop-message\") was specified"),
                evt_tag_str("input", input));
    }
  else
    result = TRUE;
  csv_scanner_deinit(&scanner);

  return result;
}

static LogPipe *
csv_parser_clone(LogPipe *s)
{
  CSVParser *self = (CSVParser *) s;
  CSVParser *cloned;

  cloned = (CSVParser *) csv_parser_new(s->cfg);
  log_parser_clone_settings(&self->super, &cloned->super);
  cloned->columns = g_list_copy_deep(self->columns, (GCopyFunc) csv_parser_column_clone, NULL);
  csv_scanner_options_copy(&cloned->options, &self->options);
  csv_parser_set_prefix(&cloned->super, self->prefix);
  csv_parser_set_on_error(&cloned->super, self->on_error);
  return &cloned->super.super;
}

static void
csv_parser_free(LogPipe *s)
{
  CSVParser *self = (CSVParser *) s;

  csv_scanner_options_clean(&self->options);
  g_free(self->prefix);
  log_parser_free_method(s);
}

static gboolean
csv_parser_init(LogPipe *s)
{
  CSVParser *self = (CSVParser *) s;

  csv_scanner_options_set_expected_columns(&self->options, g_list_length(self->columns));
  if (!csv_scanner_options_validate(&self->options))
    return FALSE;

  return log_parser_init_method(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogParser *
csv_parser_new(GlobalConfig *cfg)
{
  CSVParser *self = g_new0(CSVParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = csv_parser_init;
  self->super.super.free_fn = csv_parser_free;
  self->super.super.clone = csv_parser_clone;
  self->super.process = csv_parser_process;

  csv_scanner_options_set_delimiters(&self->options, " ");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
  return &self->super;
}

guint32
csv_parser_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "escape-none") == 0)
    return CSV_PARSER_ESCAPE_MODE_NONE;
  else if (strcmp(flag, "escape-backslash") == 0)
    return CSV_PARSER_ESCAPE_MODE_BACKSLASH;
  else if (strcmp(flag, "escape-double-char") == 0)
    return CSV_PARSER_ESCAPE_MODE_DOUBLE_CHAR;
  else if (strcmp(flag, "strip-whitespace") == 0)
    return CSV_SCANNER_STRIP_WHITESPACE;
  else if (strcmp(flag, "greedy") == 0)
    return CSV_SCANNER_GREEDY;
  else if (strcmp(flag, "drop-invalid") == 0)
    return CSV_PARSER_DROP_INVALID;
  return 0;
}

gint
csv_parser_lookup_dialect(const gchar *flag)
{
  if (strcmp(flag, "escape-none") == 0)
    return CSV_SCANNER_ESCAPE_NONE;
  else if (strcmp(flag, "escape-backslash") == 0)
    return CSV_SCANNER_ESCAPE_BACKSLASH;
  else if (strcmp(flag, "escape-backslash-with-sequences") == 0)
    return CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES;
  else if (strcmp(flag, "escape-double-char") == 0)
    return CSV_SCANNER_ESCAPE_DOUBLE_CHAR;
  return -1;
}

void
csv_parser_set_on_error(LogParser *s, gint on_error)
{
  CSVParser *self = (CSVParser *) s;
  self->on_error = on_error;
}
