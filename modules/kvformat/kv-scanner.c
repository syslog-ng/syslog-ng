/*
 * Copyright (c) 2015 Balabit
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
#include "kv-scanner.h"
#include "str-utils.h"
#include "utf8utils.h"

#include <string.h>

enum {
  KV_QUOTE_INITIAL = 0,
  KV_QUOTE_STRING,
  KV_QUOTE_BACKSLASH,
  KV_QUOTE_FINISH
};

void
kv_scanner_input(KVScanner *self, const gchar *input)
{
  self->input = input;
  self->input_len = strlen(input);
  self->input_pos = 0;
}

static gboolean
_kv_scanner_skip_space(KVScanner *self)
{
  while (self->input[self->input_pos] == ' ')
    self->input_pos++;
  return TRUE;
}

static gboolean
_is_valid_key_character(int c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         (c == '_') ||
         (c == '-');
}

static gboolean
_kv_scanner_extract_key(KVScanner *self)
{
  const gchar *input_ptr = &self->input[self->input_pos];
  const gchar *start_of_key;
  const gchar *equal;

  equal = strchr(input_ptr, '=');
  if (!equal)
    return FALSE;
  start_of_key = equal - 1;
  while (start_of_key > input_ptr && _is_valid_key_character(*start_of_key))
    start_of_key--;
  if (!_is_valid_key_character(*start_of_key))
    start_of_key++;
  g_string_assign_len(self->key, start_of_key, equal - start_of_key);
  self->input_pos = equal - self->input + 1;
  return TRUE;
}

static gboolean
_kv_scanner_extract_value(KVScanner *self)
{
  const gchar *cur;
  gchar control;

  g_string_truncate(self->value, 0);
  self->quote_state = KV_QUOTE_INITIAL;
  self->value_was_quoted = FALSE;
  cur = &self->input[self->input_pos];
  while (*cur && self->quote_state != KV_QUOTE_FINISH)
    {
      switch (self->quote_state)
        {
        case KV_QUOTE_INITIAL:
          if (*cur == ' ' || strncmp(cur, ", ", 2) == 0)
            {
              self->quote_state = KV_QUOTE_FINISH;
              break;
            }
          else if (*cur == '\"' || *cur == '\'')
            {
              self->quote_state = KV_QUOTE_STRING;
              self->quote_char = *cur;
              if (self->value->len == 0)
                self->value_was_quoted = TRUE;
              break;
            }
          g_string_append_c(self->value, *cur);
          break;
        case KV_QUOTE_STRING:
          if (*cur == self->quote_char)
            {
              self->quote_state = KV_QUOTE_INITIAL;
              break;
            }
          else if (*cur == '\\')
            {
              self->quote_state = KV_QUOTE_BACKSLASH;
              break;
            }
          g_string_append_c(self->value, *cur);
          break;
        case KV_QUOTE_BACKSLASH:
          switch (*cur)
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
              if (self->quote_char != *cur)
                g_string_append_c(self->value, '\\');
              control = *cur;
              break;
            }
          g_string_append_c(self->value, control);
          self->quote_state = KV_QUOTE_STRING;
          break;
        }
      cur++;
    }
  self->input_pos = cur - self->input;
  return TRUE;
}

static gboolean
_kv_scanner_decode_value(KVScanner *self)
{
  if (self->parse_value)
    {
      g_string_truncate(self->decoded_value, 0);
      if (self->parse_value(self))
        g_string_assign_len(self->value, self->decoded_value->str, self->decoded_value->len);
    }
  return TRUE;
}

gboolean
kv_scanner_scan_next(KVScanner *self)
{
  _kv_scanner_skip_space(self);
  if (!_kv_scanner_extract_key(self) ||
      !_kv_scanner_extract_value(self) ||
      !_kv_scanner_decode_value(self))
    return FALSE;

  return TRUE;
}

const gchar *
kv_scanner_get_current_key(KVScanner *self)
{
  return self->key->str;
}

const gchar *
kv_scanner_get_current_value(KVScanner *self)
{
  return self->value->str;
}

void
kv_scanner_free_method(KVScanner *self)
{
  g_string_free(self->key, TRUE);
  g_string_free(self->value, TRUE);
  g_string_free(self->decoded_value, TRUE);
}

/* NOTE: this is a very limited clone operation that doesn't allow
 * descendant types (e.g.  linux-audit scanner to have their own state */
KVScanner *
kv_scanner_clone(KVScanner *self)
{
  KVScanner *cloned = kv_scanner_new();
  cloned->parse_value = self->parse_value;
  return cloned;
}

void
kv_scanner_init(KVScanner *self)
{
  memset(self, 0, sizeof(*self));
  self->key = g_string_sized_new(32);
  self->value = g_string_sized_new(64);
  self->decoded_value = g_string_sized_new(64);
  self->free_fn = kv_scanner_free_method;
}

KVScanner *
kv_scanner_new(void)
{
  KVScanner *self = g_new0(KVScanner, 1);

  kv_scanner_init(self);
  return self;
}

void
kv_scanner_free(KVScanner *self)
{
  if (self->free_fn)
    self->free_fn(self);
  g_free(self);
}