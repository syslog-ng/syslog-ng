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

#include <string.h>

typedef struct _KVScannerSimple KVScannerSimple;
struct _KVScannerSimple
{
  KVScanner super;
  gint quote_state;
};

enum
{
  KV_QUOTE_INITIAL = 0,
  KV_QUOTE_STRING,
  KV_QUOTE_BACKSLASH,
  KV_QUOTE_FINISH
};

static gboolean
_extract_key(KVScannerSimple *self)
{
  const gchar *input_ptr = &self->super.input[self->super.input_pos];
  const gchar *start_of_key;
  const gchar *separator;
  gsize len;

  separator = strchr(input_ptr, self->super.value_separator);
  do
    {
      if (!separator)
        return FALSE;
      start_of_key = separator;
      while (start_of_key > input_ptr && kv_scanner_is_valid_key_character(*(start_of_key - 1)))
        start_of_key--;
      len = separator - start_of_key;
      if (len < 1)
        separator = strchr(separator + 1, self->super.value_separator);
    }
  while (len < 1);

  g_string_assign_len(self->super.key, start_of_key, len);
  self->super.input_pos = separator - self->super.input + 1;
  return TRUE;
}

static void
_decode_backslash_escape(KVScannerSimple *self, gchar ch)
{
  gchar control;
  switch (ch)
    {
    case 'b':
      control = '\b';
      break;
    case 'f':
      control = '\f';
      break;
    case 'n':
      control = '\n';
      break;
    case 'r':
      control = '\r';
      break;
    case 't':
      control = '\t';
      break;
    case '\\':
      control = '\\';
      break;
    default:
      if (self->super.quote_char != ch)
        {
          g_string_append_c(self->super.value, '\\');
        }
      control = ch;
      break;
    }
  g_string_append_c(self->super.value, control);
}

static gboolean
_is_delimiter(const gchar *cur)
{
  return (*cur == ' ') || (strncmp(cur, ", ", 2) == 0);
}

static void
_extract_value(KVScannerSimple *self)
{
  const gchar *cur;

  g_string_truncate(self->super.value, 0);
  self->super.value_was_quoted = FALSE;
  cur = &self->super.input[self->super.input_pos];

  self->quote_state = KV_QUOTE_INITIAL;
  while (*cur && self->quote_state != KV_QUOTE_FINISH)
    {
      switch (self->quote_state)
        {
        case KV_QUOTE_INITIAL:
          if (_is_delimiter(cur))
            {
              self->quote_state = KV_QUOTE_FINISH;
            }
          else if (*cur == '\"' || *cur == '\'')
            {
              self->quote_state = KV_QUOTE_STRING;
              self->super.quote_char = *cur;
              if (self->super.value->len == 0)
                self->super.value_was_quoted = TRUE;
            }
          else
            {
              g_string_append_c(self->super.value, *cur);
            }
          break;
        case KV_QUOTE_STRING:
          if (*cur == self->super.quote_char)
            self->quote_state = KV_QUOTE_INITIAL;
          else if (*cur == '\\')
            self->quote_state = KV_QUOTE_BACKSLASH;
          else
            g_string_append_c(self->super.value, *cur);
          break;
        case KV_QUOTE_BACKSLASH:
          _decode_backslash_escape(self, *cur);
          self->quote_state = KV_QUOTE_STRING;
          break;
        }
      cur++;
    }
  self->super.input_pos = cur - self->super.input;
}

static gboolean
_scan_next(KVScanner *s)
{
  KVScannerSimple *self = (KVScannerSimple *)s;

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
  return kv_scanner_simple_new(s->value_separator, s->parse_value);
}

KVScanner *
kv_scanner_simple_new(gchar value_separator, KVParseValue *parse_value)
{
  KVScannerSimple *self = g_new0(KVScannerSimple, 1);

  kv_scanner_init(&self->super, value_separator, parse_value);
  self->super.scan_next = _scan_next;
  self->super.clone = _clone;

  return &self->super;
}
