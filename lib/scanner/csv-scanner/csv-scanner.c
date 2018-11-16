/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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
#include "csv-scanner.h"
#include "str-utils.h"
#include "string-list.h"
#include "scratch-buffers.h"

#include <string.h>

/************************************************************************
 * CSVScannerOptions
 ************************************************************************/

void
csv_scanner_options_set_flags(CSVScannerOptions *options, guint32 flags)
{
  options->flags = flags;
}

void
csv_scanner_options_set_dialect(CSVScannerOptions *options, CSVScannerDialect dialect)
{
  options->dialect = dialect;
}

void
csv_scanner_options_set_columns(CSVScannerOptions *options, GList *columns)
{
  string_list_free(options->columns);
  options->columns = columns;
}

void
csv_scanner_options_set_delimiters(CSVScannerOptions *options, const gchar *delimiters)
{
  g_free(options->delimiters);
  options->delimiters = g_strdup(delimiters);
}

void
csv_scanner_options_set_string_delimiters(CSVScannerOptions *options, GList *string_delimiters)
{
  string_list_free(options->string_delimiters);
  options->string_delimiters = string_delimiters;
}

void
csv_scanner_options_set_quotes_start_and_end(CSVScannerOptions *options, const gchar *quotes_start,
                                             const gchar *quotes_end)
{
  g_free(options->quotes_start);
  g_free(options->quotes_end);
  options->quotes_start = g_strdup(quotes_start);
  options->quotes_end = g_strdup(quotes_end);
}

void
csv_scanner_options_set_quotes(CSVScannerOptions *options, const gchar *quotes)
{
  csv_scanner_options_set_quotes_start_and_end(options, quotes, quotes);
}

void
csv_scanner_options_set_quote_pairs(CSVScannerOptions *options, const gchar *quote_pairs)
{
  gint i;

  g_free(options->quotes_start);
  g_free(options->quotes_end);

  options->quotes_start = g_malloc((strlen(quote_pairs) / 2) + 1);
  options->quotes_end = g_malloc((strlen(quote_pairs) / 2) + 1);

  for (i = 0; quote_pairs[i] && quote_pairs[i+1]; i += 2)
    {
      options->quotes_start[i / 2] = quote_pairs[i];
      options->quotes_end[i / 2] = quote_pairs[i + 1];
    }
  options->quotes_start[i / 2] = 0;
  options->quotes_end[i / 2] = 0;
}


void
csv_scanner_options_set_null_value(CSVScannerOptions *options, const gchar *null_value)
{
  g_free(options->null_value);
  options->null_value = null_value && null_value[0] ? g_strdup(null_value) : NULL;
}

void
csv_scanner_options_copy(CSVScannerOptions *dst, CSVScannerOptions *src)
{
  csv_scanner_options_set_delimiters(dst, src->delimiters);
  csv_scanner_options_set_quotes_start_and_end(dst, src->quotes_start, src->quotes_end);
  csv_scanner_options_set_null_value(dst, src->null_value);
  csv_scanner_options_set_string_delimiters(dst, string_list_clone(src->string_delimiters));
  csv_scanner_options_set_columns(dst, string_list_clone(src->columns));
  dst->dialect = src->dialect;
  dst->flags = src->flags;
}

void
csv_scanner_options_clean(CSVScannerOptions *options)
{
  g_free(options->quotes_start);
  g_free(options->quotes_end);
  g_free(options->null_value);
  g_free(options->delimiters);
  string_list_free(options->string_delimiters);
  string_list_free(options->columns);
}

/************************************************************************
 * CSVScanner
 ************************************************************************/

static gboolean
_is_whitespace_char(const gchar *str)
{
  return (*str == ' ' || *str == '\t');
}

static void
_skip_whitespace(const gchar **src)
{
  while (_is_whitespace_char(*src))
    (*src)++;
}

static void
_switch_to_next_column(CSVScanner *self)
{
  if (!self->current_column && self->src && self->src[0])
    self->current_column = self->options->columns;
  else if (self->current_column)
    self->current_column = self->current_column->next;
  g_string_truncate(self->current_value, 0);
}

static gboolean
_is_last_column(CSVScanner *self)
{
  return self->current_column && self->current_column->next == NULL;
}

static gboolean
_is_at_the_end_of_columns(CSVScanner *self)
{
  return self->current_column == NULL;
}

static void
_parse_opening_quote_character(CSVScanner *self)
{
  gchar *quote = _strchr_optimized_for_single_char_haystack(self->options->quotes_start, *self->src);

  if (quote != NULL)
    {
      /* ok, quote character found */
      self->src++;
      self->current_quote = self->options->quotes_end[quote - self->options->quotes_start];
    }
  else
    {
      /* we didn't start with a quote character, no need for escaping, delimiter terminates */
      self->current_quote = 0;
    }
}

static void
_parse_left_whitespace(CSVScanner *self)
{
  if ((self->options->flags & CSV_SCANNER_STRIP_WHITESPACE) == 0)
    return;

  _skip_whitespace(&self->src);
}

static void
_parse_character_with_quotation(CSVScanner *self)
{
  /* quoted character */
  if (self->options->dialect == CSV_SCANNER_ESCAPE_BACKSLASH &&
      *self->src == '\\' &&
      *(self->src + 1))
    {
      self->src++;
    }
  else if (self->options->dialect == CSV_SCANNER_ESCAPE_DOUBLE_CHAR &&
           *self->src == self->current_quote &&
           *(self->src+1) == self->current_quote)
    {
      self->src++;
    }
  else if (*self->src == self->current_quote)
    {
      self->current_quote = 0;
      self->src++;
      return;
    }
  g_string_append_c(self->current_value, *self->src);
  self->src++;
}

/* searches for str in list and returns the first occurrence, otherwise NULL */
static const gboolean
_match_string_delimiters_at_current_position(const char *input, GList *string_delimiters, int *result_length)
{
  GList *l;

  for (l = string_delimiters; l; l = l->next)
    {
      gint len = strlen(l->data);

      if (strncmp(input, l->data, len) == 0)
        {
          *result_length = len;
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
_parse_string_delimiters_at_current_position(CSVScanner *self)
{
  gint delim_len;

  if (!self->options->string_delimiters)
    return FALSE;

  if (_match_string_delimiters_at_current_position(self->src,
                                                   self->options->string_delimiters,
                                                   &delim_len))
    {
      self->src += delim_len;
      return TRUE;
    }
  return FALSE;
}

static gboolean
_parse_character_delimiters_at_current_position(CSVScanner *self)
{
  if (_strchr_optimized_for_single_char_haystack(self->options->delimiters, *self->src) != NULL)
    {
      self->src++;
      return TRUE;
    }
  return FALSE;
}

static gboolean
_parse_delimiter(CSVScanner *self)
{
  if (_parse_string_delimiters_at_current_position(self))
    return TRUE;

  if (_parse_character_delimiters_at_current_position(self))
    return TRUE;

  return FALSE;
}

static void
_parse_unquoted_literal_character(CSVScanner *self)
{
  g_string_append_c(self->current_value, *self->src);
  self->src++;
}

static void
_parse_value_with_whitespace_and_delimiter(CSVScanner *self)
{
  while (*self->src)
    {
      if (self->current_quote)
        {
          /* within quotation marks */
          _parse_character_with_quotation(self);
        }
      else
        {
          /* unquoted value */
          if (_parse_delimiter(self))
            break;
          _parse_unquoted_literal_character(self);
        }
    }
}

static gint
_get_value_length_without_right_whitespace(CSVScanner *self)
{
  gint len = self->current_value->len;

  while (len > 0 && _is_whitespace_char(self->current_value->str + len - 1))
    len--;

  return len;
}

static void
_translate_rstrip_whitespace(CSVScanner *self)
{
  if (self->options->flags & CSV_SCANNER_STRIP_WHITESPACE)
    g_string_truncate(self->current_value, _get_value_length_without_right_whitespace(self));
}

static void
_translate_null_value(CSVScanner *self)
{
  if (self->options->null_value &&
      strcmp(self->current_value->str, self->options->null_value) == 0)
    g_string_truncate(self->current_value, 0);
}

static void
_translate_value(CSVScanner *self)
{
  _translate_rstrip_whitespace(self);
  _translate_null_value(self);
}

gboolean
csv_scanner_scan_next(CSVScanner *self)
{
  _switch_to_next_column(self);

  if (!self->src || !self->current_column)
    return FALSE;

  if (_is_last_column(self) && (self->options->flags & CSV_SCANNER_GREEDY))
    {
      g_string_assign(self->current_value, self->src);
      self->src = NULL;
      return TRUE;
    }
  else if (self->src[0] == 0)
    {
      /* no more input data and a real column, not a greedy one */
      return FALSE;
    }
  else
    {
      _parse_opening_quote_character(self);
      _parse_left_whitespace(self);
      _parse_value_with_whitespace_and_delimiter(self);
      _translate_value(self);
      return TRUE;
    }
}

const gchar *
csv_scanner_get_current_name(CSVScanner *self)
{
  return (const gchar *) self->current_column->data;
}

gboolean
csv_scanner_is_scan_complete(CSVScanner *self)
{
  /* we didn't process all of the input */
  if (self->src && self->src[0] != 0)
    return FALSE;

  return _is_at_the_end_of_columns(self);
}

void
csv_scanner_init(CSVScanner *scanner, CSVScannerOptions *options, const gchar *input)
{
  memset(scanner, 0, sizeof(*scanner));
  scanner->src = input;
  scanner->current_value = scratch_buffers_alloc();
  scanner->current_column = NULL;
  scanner->options = options;
}

void
csv_scanner_deinit(CSVScanner *self)
{
}

const gchar *
csv_scanner_get_current_value(CSVScanner *self)
{
  return self->current_value->str;
}

gint
csv_scanner_get_current_value_len(CSVScanner *self)
{
  return self->current_value->len;
}

gchar *
csv_scanner_dup_current_value(CSVScanner *self)
{
  return g_strndup(csv_scanner_get_current_value(self),
                   csv_scanner_get_current_value_len(self));
}
