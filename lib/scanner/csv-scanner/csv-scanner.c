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
#include "messages.h"

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
csv_scanner_options_set_expected_columns(CSVScannerOptions *options, gint expected_columns)
{
  options->expected_columns = expected_columns;
}

void
csv_scanner_options_copy(CSVScannerOptions *dst, CSVScannerOptions *src)
{
  csv_scanner_options_set_delimiters(dst, src->delimiters);
  csv_scanner_options_set_quotes_start_and_end(dst, src->quotes_start, src->quotes_end);
  csv_scanner_options_set_null_value(dst, src->null_value);
  csv_scanner_options_set_string_delimiters(dst, string_list_clone(src->string_delimiters));
  csv_scanner_options_set_expected_columns(dst, src->expected_columns);
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
}

gboolean
csv_scanner_options_validate(CSVScannerOptions *options)
{
  if(options->expected_columns == 0 && (options->flags & CSV_SCANNER_GREEDY))
    {
      msg_error("The greedy flag of csv-parser can not be used without specifying the columns() option");
      return FALSE;
    }

  return TRUE;
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

static gint
_decode_xdigit(gchar xdigit)
{
  if (xdigit >= '0' && xdigit <= '9')
    return xdigit - '0';
  if (xdigit >= 'a' && xdigit <= 'f')
    return xdigit - 'a' + 10;
  if (xdigit >= 'A' && xdigit <= 'F')
    return xdigit - 'A' + 10;
  return -1;
}

static gint
_decode_xbyte(gchar xdigit1, gchar xdigit2)
{
  gint nibble_hi, nibble_lo;

  nibble_hi = _decode_xdigit(xdigit1);
  nibble_lo = _decode_xdigit(xdigit2);
  if (nibble_hi < 0 || nibble_lo < 0)
    return -1;
  return (nibble_hi << 4) + nibble_lo;
}

static void
_parse_character_with_quotation(CSVScanner *self)
{
  gchar ch;
  /* quoted character */
  if (self->options->dialect == CSV_SCANNER_ESCAPE_BACKSLASH &&
      *self->src == '\\' &&
      *(self->src + 1))
    {
      self->src++;
      ch = *self->src;
    }
  else if (self->options->dialect == CSV_SCANNER_ESCAPE_BACKSLASH_WITH_SEQUENCES &&
           *self->src == '\\' &&
           *(self->src + 1))
    {
      self->src++;
      ch = *self->src;
      if (ch != self->current_quote)
        {
          switch (ch)
            {
            case 'a':
              ch = '\a';
              break;
            case 'n':
              ch = '\n';
              break;
            case 'r':
              ch = '\r';
              break;
            case 't':
              ch = '\t';
              break;
            case 'v':
              ch = '\v';
              break;
            case 'x':
              if (*(self->src+1) && *(self->src+2))
                {
                  gint decoded = _decode_xbyte(*(self->src+1), *(self->src+2));
                  if (decoded >= 0)
                    {
                      self->src += 2;
                      ch = decoded;
                    }
                  else
                    ch = 'x';
                }
              break;
            default:
              break;
            }
        }
    }
  else if (self->options->dialect == CSV_SCANNER_ESCAPE_DOUBLE_CHAR &&
           *self->src == self->current_quote &&
           *(self->src+1) == self->current_quote)
    {
      self->src++;
      ch = *self->src;
    }
  else if (*self->src == self->current_quote)
    {
      self->current_quote = 0;
      self->src++;
      return;
    }
  else
    {
      ch = *self->src;
    }
  g_string_append_c(self->current_value, ch);
  self->src++;
}

/* searches for str in list and returns the first occurrence, otherwise NULL */
static gboolean
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

static gboolean
_is_last_column(CSVScanner *self)
{
  if (self->options->expected_columns == 0)
    return FALSE;

  if (self->current_column == self->options->expected_columns - 1)
    return TRUE;

  return FALSE;
}

static gboolean
_switch_to_next_column(CSVScanner *self)
{
  g_string_truncate(self->current_value, 0);

  if (self->options->expected_columns == 0)
    return TRUE;

  switch (self->state)
    {
    case CSV_STATE_INITIAL:
      self->state = CSV_STATE_COLUMNS;
      self->current_column = 0;
      if (self->options->expected_columns > 0)
        return TRUE;
      self->state = CSV_STATE_FINISH;
      return FALSE;
    case CSV_STATE_COLUMNS:
    case CSV_STATE_GREEDY_COLUMN:
      self->current_column++;
      if (self->current_column <= self->options->expected_columns - 1)
        return TRUE;
      self->state = CSV_STATE_FINISH;
      return FALSE;
    case CSV_STATE_PARTIAL_INPUT:
    case CSV_STATE_FINISH:
      return FALSE;
    default:
      break;
    }
  g_assert_not_reached();
}

gboolean
csv_scanner_scan_next(CSVScanner *self)
{
  if (!_switch_to_next_column(self))
    return FALSE;

  if (_is_last_column(self) && (self->options->flags & CSV_SCANNER_GREEDY))
    {
      _parse_left_whitespace(self);
      g_string_assign(self->current_value, self->src);
      self->src += self->current_value->len;
      self->state = CSV_STATE_GREEDY_COLUMN;
      _translate_value(self);
      return TRUE;
    }
  else if (self->src[0] == 0)
    {
      /* no more input data and a real column, not a greedy one */
      self->state = self->options->expected_columns == 0 ? CSV_STATE_FINISH : CSV_STATE_PARTIAL_INPUT;
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

gint
csv_scanner_get_current_column(CSVScanner *self)
{
  return self->current_column;
}

gboolean
csv_scanner_is_scan_complete(CSVScanner *self)
{
  /* we didn't process all of the input */
  if (self->src[0] != 0)
    return FALSE;

  return self->state == CSV_STATE_FINISH;
}

void
csv_scanner_init(CSVScanner *scanner, CSVScannerOptions *options, const gchar *input)
{
  memset(scanner, 0, sizeof(*scanner));
  scanner->state = CSV_STATE_INITIAL;
  scanner->src = input;
  scanner->current_value = scratch_buffers_alloc();
  scanner->current_column = 0;
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
