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
#ifndef KV_SCANNER_H_INCLUDED
#define KV_SCANNER_H_INCLUDED

#include "syslog-ng.h"
#include "str-utils.h"


typedef struct _KVScanner KVScanner;

typedef gboolean (*KVTransformValueFunc)(KVScanner *);

struct _KVScanner
{
  const gchar *input;
  gsize input_pos;
  GString *key;
  GString *value;
  GString *decoded_value;
  gboolean value_was_quoted;
  gchar value_separator;
  gchar quote_char;
  gboolean allow_space;
  KVTransformValueFunc transform_value;
  gboolean (*scan_next)(KVScanner *self);
  KVScanner* (*clone)(KVScanner *self);
};

void kv_scanner_init(KVScanner *self, gchar value_separator, KVTransformValueFunc transform_value);
void kv_scanner_free(KVScanner *self);

static inline void
kv_scanner_input(KVScanner *self, const gchar *input)
{
  self->input = input;
  self->input_pos = 0;
}

static inline KVScanner *
kv_scanner_clone(KVScanner *self)
{
  return self->clone(self);
}

static inline gboolean
kv_scanner_scan_next(KVScanner *self)
{
  return self->scan_next(self);
}

static inline const gchar *
kv_scanner_get_current_key(KVScanner *self)
{
  return self->key->str;
}

static inline const gchar *
kv_scanner_get_current_value(KVScanner *self)
{
  return self->value->str;
}

static inline gboolean
kv_scanner_is_valid_key_character(gchar c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         (c == '_') ||
         (c == '-');
}

static inline void
kv_scanner_transform_value(KVScanner *self)
{
  if (self->transform_value)
    {
      g_string_truncate(self->decoded_value, 0);
      if (self->transform_value(self))
        g_string_assign_len(self->value, self->decoded_value->str, self->decoded_value->len);
    }
}

static inline void
kv_scanner_set_transform_value(KVScanner *self, KVTransformValueFunc transform_value)
{
  self->transform_value = transform_value;
}

KVScanner* kv_scanner_new(gchar value_separator, KVTransformValueFunc transform_value, gboolean allow_space);

#endif
