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
#include "kv-scanner-simple.h"
#include "utf8utils.h"
#include "kv-scanner.h"
#include "str-repr/decode.h"

#include <string.h>

typedef struct _KVScannerSimple KVScannerSimple;
struct _KVScannerSimple
{
  KVScanner super;
  gboolean allow_space;
};

static inline const gchar *
_locate_separator(KVScannerSimple *self, const gchar *start)
{
  return strchr(start, self->super.value_separator);
}

static inline void
_locate_start_of_key(KVScannerSimple *self, const gchar *end_of_key, const gchar **start_of_key)
{
  const gchar *input = &self->super.input[self->super.input_pos];
  const gchar *cur;

  cur = end_of_key;
  while (cur > input && kv_scanner_is_valid_key_character(*(cur - 1)))
    cur--;
  *start_of_key = cur;
}

static inline void
_locate_end_of_key(KVScannerSimple *self, const gchar *separator, const gchar **end_of_key)
{
  const gchar *input = &self->super.input[self->super.input_pos];
  const gchar *cur;

  /* this function locates the character pointing right next to the end of
   * the key, e.g. with this input
   *   foo   = bar
   *
   * it would start with the '=' sign and skip spaces backwards, to locate
   * the space right next to "foo" */

  cur = separator;
  if (self->allow_space)
    {
      while (cur > input && (*(cur - 1)) == ' ')
        cur--;
    }
  *end_of_key = cur;
}

static inline gboolean
_extract_key_from_positions(KVScannerSimple *self, const gchar *start_of_key, const gchar *end_of_key)
{
  gint len = end_of_key - start_of_key;

  if (len >= 1)
    {
      g_string_assign_len(self->super.key, start_of_key, len);
      return TRUE;
    }
  return FALSE;
}

static gboolean
_extract_key(KVScannerSimple *self)
{
  const gchar *input = &self->super.input[self->super.input_pos];
  const gchar *start_of_key, *end_of_key;
  const gchar *separator;

  separator = _locate_separator(self, input);
  while (separator)
    {
      _locate_end_of_key(self, separator, &end_of_key);
      _locate_start_of_key(self, end_of_key, &start_of_key);

      if (_extract_key_from_positions(self, start_of_key, end_of_key))
        {
          self->super.input_pos = separator - self->super.input + 1;
          return TRUE;
        }
      separator = _locate_separator(self, separator + 1);
    }

  return FALSE;
}

static gboolean
_is_quoted(const gchar *input)
{
  return *input == '\'' || *input == '\"';
}

static gboolean
_key_follows(KVScannerSimple *self, const gchar *cur)
{
  const gchar *key = cur;

  while (kv_scanner_is_valid_key_character(*key))
    key++;

  if (self->allow_space)
    {
      while (*key == ' ')
        key++;
    }
  return (key != cur) && (*key == self->super.value_separator);
}

static inline void
_skip_spaces(const gchar **input)
{
  const gchar *cur = *input;

  while (*cur == ' ')
    cur++;
  *input = cur;
}

static inline gboolean
_end_of_string(const gchar *cur)
{
  return *cur == 0;
}

static gboolean
_match_delimiter(const gchar *cur, const gchar **new_cur, gpointer user_data)
{
  KVScannerSimple *self = (gpointer) user_data;
  gboolean result = FALSE;

  if (!self->super.value_was_quoted &&
      self->allow_space &&
      *cur == ' ')
    {
      _skip_spaces(&cur);

      if (_end_of_string(cur) ||
          _key_follows(self, cur))
        {
          *new_cur = cur;
          result = TRUE;
        }
    }
  else
    {
      result = (*cur == ' ') || (strncmp(cur, ", ", 2) == 0);
      *new_cur = cur + 1;
    }
  return result;
}

static inline void
_skip_initial_spaces(KVScannerSimple *self)
{
  if (self->allow_space)
    {
      const gchar *input = &self->super.input[self->super.input_pos];
      const gchar *end;

      while (*input == ' ' && !_match_delimiter(input, &end, self))
        input++;
      self->super.input_pos = input - self->super.input;
    }
}

static inline void
_decode_value(KVScannerSimple *self)
{
  const gchar *input = &self->super.input[self->super.input_pos];
  const gchar *end;
  StrReprDecodeOptions options = {
    0,
    .match_delimiter = _match_delimiter,
    .match_delimiter_data = self,
  };

  self->super.value_was_quoted = _is_quoted(input);
  if (str_repr_decode_with_options(self->super.value, input, &end, &options))
    {
      self->super.input_pos = end - self->super.input;
    }
  else
    {
      /* quotation error, set was_quoted to FALSE */
      self->super.value_was_quoted = FALSE;
    }
}

static void
_extract_value(KVScannerSimple *self)
{
  _skip_initial_spaces(self);
  _decode_value(self);
}

static gboolean
_scan_next(KVScanner *s)
{
  KVScannerSimple *self = (KVScannerSimple *)s;

  if (!_extract_key(self))
    return FALSE;

  _extract_value(self);
  kv_scanner_transform_value(s);

  return TRUE;
}

static KVScanner *
_clone(KVScanner *s)
{
  KVScannerSimple *self = (KVScannerSimple *) s;
  return kv_scanner_simple_new(s->value_separator, s->transform_value, self->allow_space);
}

KVScanner *
kv_scanner_simple_new(gchar value_separator, KVTransformValueFunc transform_value, gboolean allow_space)
{
  KVScannerSimple *self = g_new0(KVScannerSimple, 1);

  kv_scanner_init(&self->super, value_separator, transform_value);
  self->super.scan_next = _scan_next;
  self->super.clone = _clone;
  self->allow_space = allow_space;

  return &self->super;
}
