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

#include <string.h>

typedef struct _CSVParser
{
  LogParser super;
  CSVScannerOptions options;
  CSVScanner scanner;
  GString *formatted_key;
  gchar *prefix;
  gint prefix_len;
} CSVParser;

#define _ESCAPE_MODE_SHIFT 16
#define _ESCAPE_MODE_MASK  0xFFFF0000

#define _ESCAPE_MODE_NONE          (1 << _ESCAPE_MODE_SHIFT)
#define _ESCAPE_MODE_BACKSLASH     (2 << _ESCAPE_MODE_SHIFT)
#define _ESCAPE_MODE_DOUBLE_CHAR   (4 << _ESCAPE_MODE_SHIFT)

CSVScannerOptions *
csv_parser_get_scanner_options(LogParser *s)
{
  CSVParser *self = (CSVParser *) s;

  return &self->options;
}

gboolean
csv_parser_set_flags(LogParser *s, guint32 flags)
{
  CSVParser *self = (CSVParser *) s;
  guint32 dialect = (flags & _ESCAPE_MODE_MASK);
  guint32 scanner_flags = flags & 0xFFFF;

  csv_scanner_options_set_flags(&self->options, scanner_flags);
  switch (dialect)
    {
    case 0:
      break;
    case _ESCAPE_MODE_NONE:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
      break;
    case _ESCAPE_MODE_BACKSLASH:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_BACKSLASH);
      break;
    case _ESCAPE_MODE_DOUBLE_CHAR:
      csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_DOUBLE_CHAR);
      break;
    default:
      return FALSE;
    }
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

static const gchar *
_get_formatted_key(CSVParser *self, const gchar *key)
{
  if (!self->prefix)
    return key;
  else if (self->formatted_key->len > 0)
    g_string_truncate(self->formatted_key, self->prefix_len);
  else
    g_string_assign(self->formatted_key, self->prefix);
  g_string_append(self->formatted_key, key);
  return self->formatted_key->str;
}

static gboolean
csv_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  CSVParser *self = (CSVParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);

  csv_scanner_input(&self->scanner, input);
  while (csv_scanner_scan_next(&self->scanner))
    {

      log_msg_set_value_by_name(msg,
                                _get_formatted_key(self, csv_scanner_get_current_name(&self->scanner)),
                                csv_scanner_get_current_value(&self->scanner),
                                csv_scanner_get_current_value_len(&self->scanner));
    }

  return csv_scanner_is_scan_finished(&self->scanner);
}

static LogPipe *
csv_parser_clone(LogPipe *s)
{
  CSVParser *self = (CSVParser *) s;
  CSVParser *cloned;

  cloned = (CSVParser *) csv_parser_new(s->cfg);
  csv_scanner_options_copy(&cloned->options, &self->options);
  cloned->super.template = log_template_ref(self->super.template);
  csv_parser_set_prefix(&cloned->super, self->prefix);
  return &cloned->super.super;
}

static void
csv_parser_free(LogPipe *s)
{
  CSVParser *self = (CSVParser *) s;

  csv_scanner_options_clean(&self->options);
  csv_scanner_state_clean(&self->scanner);
  g_string_free(self->formatted_key, TRUE);
  g_free(self->prefix);
  log_parser_free_method(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogParser *
csv_parser_new(GlobalConfig *cfg)
{
  CSVParser *self = g_new0(CSVParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = csv_parser_free;
  self->super.super.clone = csv_parser_clone;
  self->super.process = csv_parser_process;
  self->formatted_key = g_string_sized_new(32);
  csv_scanner_options_set_delimiters(&self->options, " ");
  csv_scanner_options_set_quote_pairs(&self->options, "\"\"''");
  csv_scanner_options_set_flags(&self->options, CSV_SCANNER_STRIP_WHITESPACE);
  csv_scanner_options_set_dialect(&self->options, CSV_SCANNER_ESCAPE_NONE);
  csv_scanner_state_init(&self->scanner, &self->options);
  return &self->super;
}

guint32
csv_parser_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "escape-none") == 0)
    return _ESCAPE_MODE_NONE;
  else if (strcmp(flag, "escape-backslash") == 0)
    return _ESCAPE_MODE_BACKSLASH;
  else if (strcmp(flag, "escape-double-char") == 0)
    return _ESCAPE_MODE_DOUBLE_CHAR;
  else if (strcmp(flag, "strip-whitespace") == 0)
    return CSV_SCANNER_STRIP_WHITESPACE;
  else if (strcmp(flag, "greedy") == 0)
    return CSV_SCANNER_GREEDY;
  else if (strcmp(flag, "drop-invalid") == 0)
    return CSV_SCANNER_DROP_INVALID;
  return 0;
}

gint
csv_parser_lookup_dialect(const gchar *flag)
{
  if (strcmp(flag, "escape-none") == 0)
    return CSV_SCANNER_ESCAPE_NONE;
  else if (strcmp(flag, "escape-backslash") == 0)
    return CSV_SCANNER_ESCAPE_BACKSLASH;
  else if (strcmp(flag, "escape-double-char") == 0)
    return CSV_SCANNER_ESCAPE_DOUBLE_CHAR;
  return -1;
}
