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
#include "kv-scanner-generic.h"
#include "kv-scanner.h"

typedef struct _KVToken KVToken;
struct _KVToken
{
  const gchar *begin;
  const gchar *end;
};

typedef struct _KVScannerGeneric KVScannerGeneric;
struct _KVScannerGeneric
{
  KVScanner super;
  gint state;
  KVToken next_key;
  KVToken next_value;
};

enum
{
  KV_INIT = 0,
  KV_SEPARATOR_AFTER_INIT,
  KV_IN_VALUE,
  KV_IN_KEY_OR_VALUE,
  KV_IN_QUOTE,
  KV_AFTER_QUOTE,
  KV_IN_SEPARATOR,
  KV_KEY_FOUND,
  KV_EOL
};

static inline void
_next_step(KVScannerGeneric *self)
{
  self->super.input_pos++;
}

static inline const gchar *
_get_current_position(KVScannerGeneric *self)
{
  return self->super.input + self->super.input_pos;
}

static inline const gchar
_get_current_char(KVScannerGeneric *self)
{
  return *(_get_current_position(self));
}

static inline gboolean
_set_current_position(KVScannerGeneric *self, const gchar *pos)
{
  if (pos < _get_current_position(self))
    {
      return FALSE;
    }

  self->super.input_pos = pos - _get_current_position(self);
  return TRUE;
}

static void
_reset_value(KVScannerGeneric *self)
{
  g_string_truncate(self->super.value, 0);
  self->next_key.begin = NULL;
  self->next_key.end = NULL;
  self->next_value.begin = _get_current_position(self);
  self->next_value.end = _get_current_position(self);
  self->super.value_was_quoted = FALSE;
}

static inline void
_extend_value_with_next_key(KVScannerGeneric *self)
{
  if (self->next_key.begin < self->next_key.end)
    {
      self->next_value.end = self->next_key.end;
      self->next_key.begin = self->next_key.end = NULL;
    }
}

static inline void
_start_next_key(KVScannerGeneric *self)
{
  self->next_key.begin = self->next_key.end = _get_current_position(self);
}

static inline void
_end_next_key(KVScannerGeneric *self)
{
  self->next_key.end = _get_current_position(self);
}

static inline void
_start_value(KVScannerGeneric *self)
{
  self->next_value.begin = self->next_value.end = _get_current_position(self);
}

static inline void
_end_value(KVScannerGeneric *self)
{
  self->next_value.end = _get_current_position(self);
}

static inline void
_init_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
      self->state = KV_SEPARATOR_AFTER_INIT;
    }
  else if (ch == '\'' || ch == '\"')
    {
      _start_value(self);
      self->super.quote_char = ch;
      self->state = KV_IN_QUOTE;
    }
  else
    {
      _start_value(self);
      self->state = KV_IN_VALUE;
    }
}

static inline void
_separator_after_init_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
    }
  else if (ch == '\'' || ch == '\"')
    {
      _start_value(self);
      self->super.quote_char = ch;
      self->state = KV_IN_QUOTE;
    }
  else if (kv_scanner_is_valid_key_character(ch))
    {
      _start_value(self);
      _start_next_key(self);
      self->state = KV_IN_KEY_OR_VALUE;
    }
  else
    {
      _start_value(self);
      self->state = KV_IN_VALUE;
    }
}

static inline void
_in_value_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      _end_value(self);
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
      _end_value(self);
      self->state = KV_IN_SEPARATOR;
    }
}

static inline void
_in_key_or_value_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      _end_next_key(self);
      _extend_value_with_next_key(self);
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
      _end_next_key(self);
      self->state = KV_IN_SEPARATOR;
    }
  else if (ch == self->super.value_separator)
    {
      _end_next_key(self);
      self->state = KV_KEY_FOUND;
    }
  else if (kv_scanner_is_valid_key_character(ch))
    {
    }
  else
    {
      _extend_value_with_next_key(self);
      self->state = KV_IN_VALUE;
    }
}

static inline void
_in_quote_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      _end_value(self);
      self->state = KV_EOL;
    }
  else if (ch == self->super.quote_char)
    {
      _end_value(self);
      self->state = KV_AFTER_QUOTE;
    }
}

static inline void
_after_quote_state(KVScannerGeneric *self, gchar ch)
{
  _end_value(self);
  if (ch == '\0')
    {
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
      self->state = KV_IN_SEPARATOR;
    }
  else if (ch == '\'' || ch == '\"')
    {
      self->super.quote_char = ch;
      self->state = KV_IN_QUOTE;
    }
  else if (kv_scanner_is_valid_key_character(ch))
    {
      _start_next_key(self);
      self->state = KV_IN_KEY_OR_VALUE;
    }
  else
    {
      self->state = KV_IN_VALUE;
    }
}

static inline void
_in_separator_state(KVScannerGeneric *self, gchar ch)
{
  if (ch == '\0')
    {
      _extend_value_with_next_key(self);
      self->state = KV_EOL;
    }
  else if (ch == ' ')
    {
    }
  else if (ch == self->super.value_separator)
    {
      self->state = KV_KEY_FOUND;
    }
  else if (kv_scanner_is_valid_key_character(ch))
    {
      _extend_value_with_next_key(self);
      _start_next_key(self);
      self->state = KV_IN_KEY_OR_VALUE;
    }
  else
    {
      _extend_value_with_next_key(self);
      self->state = KV_IN_VALUE;
    }
}

static inline void
_produce_value(KVScannerGeneric *self)
{
  const gchar *cur;

  if (*self->next_value.begin == self->super.quote_char && *(self->next_value.end - 1) == self->super.quote_char)
    {
      self->super.value_was_quoted = TRUE;
      self->next_value.begin++;
      self->next_value.end--;
    }

  for (cur = self->next_value.begin; cur < self->next_value.end; cur++)
    {
      g_string_append_c(self->super.value, *cur);
    }
}

static inline void
_extract_value(KVScannerGeneric *self)
{
  gchar cur_char;

  _reset_value(self);
  self->state = KV_INIT;

  do
    {
      cur_char = _get_current_char(self);
      switch (self->state)
        {
        case KV_INIT:
          _init_state(self, cur_char);
          break;
        case KV_SEPARATOR_AFTER_INIT:
          _separator_after_init_state(self, cur_char);
          break;
        case KV_IN_VALUE:
          _in_value_state(self, cur_char);
          break;
        case KV_IN_KEY_OR_VALUE:
          _in_key_or_value_state(self, cur_char);
          break;
        case KV_IN_QUOTE:
          _in_quote_state(self, cur_char);
          break;
        case KV_AFTER_QUOTE:
          _after_quote_state(self, cur_char);
          break;
        case KV_IN_SEPARATOR:
          _in_separator_state(self, cur_char);
          break;
        }
      _next_step(self);
    }
  while (self->state != KV_KEY_FOUND &&
         self->state != KV_EOL);

  _produce_value(self);
}

static inline gboolean
_find_first_key(KVScannerGeneric *self)
{
  const gchar *separator, *start_of_key, *end_of_key;
  gint len;

  separator = strchr(self->super.input, self->super.value_separator);

  do
    {
      if (!separator)
        return FALSE;

      end_of_key = separator - 1;
      while (end_of_key >= self->super.input && *end_of_key == ' ')
        end_of_key--;

      start_of_key = end_of_key;

      while (start_of_key >= self->super.input && kv_scanner_is_valid_key_character(*start_of_key))
        start_of_key--;

      len = end_of_key - start_of_key;

      if (len < 1)
        separator = strchr(separator + 1, self->super.value_separator);
    }
  while (len < 1);

  self->next_key.begin = start_of_key + 1;
  self->next_key.end = end_of_key + 1;

  return _set_current_position(self, separator + 1);
}

static inline gboolean
_extract_key(KVScannerGeneric *self)
{

  if (self->state == KV_EOL || _get_current_char(self) == '\0')
    {
      return FALSE;
    }

  if ((self->next_key.begin == NULL || self->next_key.end == NULL) && !_find_first_key(self))
    {
      return FALSE;
    }

  g_string_assign_len(self->super.key, self->next_key.begin, self->next_key.end - self->next_key.begin);

  return TRUE;
}

static gboolean
_scan_next(KVScanner *s)
{
  KVScannerGeneric *self = (KVScannerGeneric *)s;

  if (!_extract_key(self))
    {
      return FALSE;
    }

  _extract_value(self);
  kv_scanner_decode_value(s);

  return TRUE;
}

static KVScanner *
_clone(KVScanner *s)
{
  return kv_scanner_generic_new(s->value_separator, s->parse_value);
}

KVScanner *
kv_scanner_generic_new(gchar value_separator, KVParseValue *parse_value)
{
  KVScannerGeneric *self = g_new0(KVScannerGeneric, 1);

  kv_scanner_init(&self->super, value_separator, parse_value);
  self->super.scan_next = _scan_next;
  self->super.clone = _clone;

  return &self->super;
}
