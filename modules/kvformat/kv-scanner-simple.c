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

static gboolean
_extract_key(KVScannerSimple *self)
{
  const gchar *input_ptr = &self->super.input[self->super.input_pos];
  const gchar *start_of_key, *end_of_key;
  const gchar *separator;
  gsize len;

  separator = strchr(input_ptr, self->super.value_separator);
  do
    {
      if (!separator)
        return FALSE;
      end_of_key = separator;
      if  (self->allow_space)
        {
          while (end_of_key > input_ptr && (*(end_of_key - 1)) == ' ')
            end_of_key--;
        }
      start_of_key = end_of_key;
      while (start_of_key > input_ptr && kv_scanner_is_valid_key_character(*(start_of_key - 1)))
        start_of_key--;
      len = end_of_key - start_of_key;
      if (len < 1)
        separator = strchr(separator + 1, self->super.value_separator);
    }
  while (len < 1);

  g_string_assign_len(self->super.key, start_of_key, len);
  self->super.input_pos = separator - self->super.input + 1;
  return TRUE;
}

static gboolean
_is_quoted(const gchar *input)
{
  return *input == '\'' || *input == '\"';
}

static gboolean
_seems_to_be_a_key_following(KVScannerSimple *self, const gchar *cur)
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

static gboolean
_match_delimiter(const gchar *cur, const gchar **new_cur, gpointer user_data)
{
  KVScannerSimple *self = (gpointer) user_data;
  gboolean result;

  if (!self->super.value_was_quoted && self->allow_space)
    {
      if (*cur == ' ')
        {
          while (*cur == ' ')
            cur++;

          if (*cur == 0 ||
              _seems_to_be_a_key_following(self, cur))
            {
              *new_cur = cur;
              return TRUE;
            }
          else
            {
              /* no this is still part of the value */
              return FALSE;
            }
        }
    }

  result = (*cur == ' ') || (strncmp(cur, ", ", 2) == 0);
  *new_cur = cur + 1;
  return result;
}

static inline void
_skip_initial_spaces(KVScannerSimple *self)
{
  const gchar *input = &self->super.input[self->super.input_pos];

  if (self->allow_space)
    {
      const gchar *end;

      while (*input == ' ' && !_match_delimiter(input, &end, self))
        input++;
    }
  self->super.input_pos = input - self->super.input;
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
