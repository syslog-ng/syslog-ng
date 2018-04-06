/*
 * Copyright (c) 2015-2017 Balabit
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
#include "kv-scanner.h"
#include "str-repr/decode.h"
#include "str-repr/encode.h"
#include "scratch-buffers.h"
#include <string.h>

static inline gboolean
_is_valid_key_character(gchar c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         (c == '_') ||
         (c == '.') ||
         (c == '-');
}

static inline const gchar *
_locate_separator(KVScanner *self, const gchar *start)
{
  return strchr(start, self->value_separator);
}

static inline void
_locate_start_of_key(KVScanner *self, const gchar *end_of_key, const gchar **start_of_key)
{
  const gchar *input = &self->input[self->input_pos];
  const gchar *cur;

  cur = end_of_key;
  while (cur > input && self->is_valid_key_character(*(cur - 1)))
    cur--;
  *start_of_key = cur;
}

static inline void
_locate_end_of_key(KVScanner *self, const gchar *separator, const gchar **end_of_key)
{
  const gchar *input = &self->input[self->input_pos];
  const gchar *cur;

  /* this function locates the character pointing right next to the end of
   * the key, e.g. with this input
   *   foo   = bar
   *
   * it would start with the '=' sign and skip spaces backwards, to locate
   * the space right next to "foo" */

  cur = separator;
  while (cur > input && (*(cur - 1)) == ' ')
    cur--;
  *end_of_key = cur;
}

static inline gboolean
_extract_key_from_positions(KVScanner *self, const gchar *start_of_key, const gchar *end_of_key)
{
  gint len = end_of_key - start_of_key;

  if (len >= 1)
    {
      g_string_assign_len(self->key, start_of_key, len);
      return TRUE;
    }
  return FALSE;
}

static inline void
_extract_stray_word(KVScanner *self, const gchar *stray_word, gssize len)
{
  if (len < 0)
    len = strlen(stray_word);
  if (self->stray_words && len > 0)
    {
      while (len > 0 && stray_word[len - 1] == ' ')
        len--;
      while (len > 0 && stray_word[0] == ' ')
        {
          stray_word++;
          len--;
        }
      if (len > 0)
        {
          if (self->stray_words->len)
            g_string_append_c(self->stray_words, ',');

          str_repr_encode_append(self->stray_words, stray_word, len, ",");
        }
    }
}

static gboolean
_should_stop(KVScanner *self)
{
  const gchar *input = &self->input[self->input_pos];
  return *input == self->stop_char;
}

static gboolean
_extract_key(KVScanner *self)
{
  const gchar *input = &self->input[self->input_pos];
  const gchar *start_of_key, *end_of_key;
  const gchar *separator;

  separator = _locate_separator(self, input);
  while (separator)
    {
      _locate_end_of_key(self, separator, &end_of_key);
      _locate_start_of_key(self, end_of_key, &start_of_key);

      if (_extract_key_from_positions(self, start_of_key, end_of_key))
        {
          _extract_stray_word(self, input, start_of_key - input);
          self->input_pos = separator - self->input + 1;
          return TRUE;
        }
      separator = _locate_separator(self, separator + 1);
    }
  _extract_stray_word(self, input, -1);
  return FALSE;
}

static gboolean
_is_quoted(const gchar *input)
{
  return *input == '\'' || *input == '\"';
}

static gboolean
_key_follows(KVScanner *self, const gchar *cur)
{
  const gchar *key = cur;

  while (self->is_valid_key_character(*key))
    key++;

  while (*key == ' ')
    key++;
  return (key != cur) && (*key == self->value_separator);
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

static inline gboolean
_pair_separator(KVScanner *self, const gchar *cur, const gchar **new_cur)
{
  if (self->pair_separator && (strncmp(cur, self->pair_separator, self->pair_separator_len) == 0))
    {
      *new_cur = cur + self->pair_separator_len;
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
_pair_separator_starts_with_a_space(KVScanner *self)
{
  return (self->pair_separator && self->pair_separator[0] == ' ');
}

static gboolean
_match_delimiter(const gchar *cur, const gchar **new_cur, gpointer user_data)
{
  KVScanner *self = (gpointer) user_data;
  gboolean result = FALSE;

  if (!self->value_was_quoted &&
      *cur == ' ')
    {
      if (_pair_separator_starts_with_a_space(self) &&
          _pair_separator(self, cur, new_cur))
        {
          result = TRUE;
        }
      else
        {
          _skip_spaces(&cur);

          if (_end_of_string(cur) ||
              _key_follows(self, cur))
            {
              *new_cur = cur;
              result = TRUE;
            }
          else if (_pair_separator(self, cur, new_cur))
            {
              result = TRUE;
            }
        }
    }
  else if (*cur == ' ')
    {
      result = TRUE;
      *new_cur = cur + 1;
    }
  else if (*cur == self->stop_char)
    {
      result = TRUE;
      *new_cur = cur;
    }
  else
    {
      result = _pair_separator(self, cur, new_cur);
    }
  return result;
}

static inline void
_skip_initial_spaces(KVScanner *self)
{
  const gchar *input = &self->input[self->input_pos];
  const gchar *end;

  while (*input == ' ' && !_match_delimiter(input, &end, self))
    input++;
  self->input_pos = input - self->input;
}

static inline void
_decode_value(KVScanner *self)
{
  const gchar *input = &self->input[self->input_pos];
  const gchar *end;
  StrReprDecodeOptions options =
  {
    .match_delimiter = _match_delimiter,
    .match_delimiter_data = self,
    .delimiter_chars = { ' ', self->pair_separator[0], self->stop_char },
  };

  self->value_was_quoted = _is_quoted(input);
  if (str_repr_decode_with_options(self->value, input, &end, &options))
    {
      self->input_pos = end - self->input;
    }
  else
    {
      /* quotation error, set was_quoted to FALSE */
      self->value_was_quoted = FALSE;
    }
}

static void
_extract_optional_annotation(KVScanner *self)
{
  if (self->extract_annotation)
    self->extract_annotation(self);
}

static void
_extract_value(KVScanner *self)
{
  self->value_was_quoted = FALSE;
  _skip_initial_spaces(self);
  _decode_value(self);
}

static inline void
_transform_value(KVScanner *self)
{
  if (self->transform_value)
    {
      g_string_truncate(self->decoded_value, 0);
      if (self->transform_value(self))
        g_string_assign_len(self->value, self->decoded_value->str, self->decoded_value->len);
    }
}

gboolean
kv_scanner_scan_next(KVScanner *s)
{
  KVScanner *self = (KVScanner *)s;

  if (_should_stop(self))
    return FALSE;

  if (!_extract_key(self))
    return FALSE;

  _extract_optional_annotation(self);

  _extract_value(self);
  _transform_value(s);

  return TRUE;
}

void
kv_scanner_deinit(KVScanner *self)
{
}

void
kv_scanner_init(KVScanner *self, gchar value_separator, const gchar *pair_separator,
                gboolean extract_stray_words)
{
  memset(self, 0, sizeof(*self));
  self->key = scratch_buffers_alloc();
  self->value = scratch_buffers_alloc();
  self->decoded_value = scratch_buffers_alloc();
  if (extract_stray_words)
    self->stray_words = scratch_buffers_alloc();
  self->value_separator = value_separator;
  self->pair_separator = pair_separator ? : ", ";
  self->pair_separator_len = strlen(self->pair_separator);
  self->is_valid_key_character = _is_valid_key_character;
  self->stop_char = 0;
}
