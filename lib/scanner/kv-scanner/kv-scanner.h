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
#ifndef KV_SCANNER_H_INCLUDED
#define KV_SCANNER_H_INCLUDED

#include "syslog-ng.h"
#include "str-utils.h"


typedef struct _KVScanner KVScanner;

typedef gboolean (*KVTransformValueFunc)(KVScanner *);
typedef void (*KVExtractAnnotationFunc)(KVScanner *);
typedef gboolean (*KVIsValidKeyCharFunc)(gchar c);

struct _KVScanner
{
  const gchar *input;
  gsize input_pos;
  GString *key;
  GString *value;
  GString *decoded_value;
  GString *stray_words;
  gboolean value_was_quoted;
  gchar value_separator;
  const gchar *pair_separator;
  gsize pair_separator_len;
  gchar stop_char;

  KVTransformValueFunc transform_value;
  KVExtractAnnotationFunc extract_annotation;
  KVIsValidKeyCharFunc is_valid_key_character;
};

void kv_scanner_init(KVScanner *self, gchar value_separator, const gchar *pair_separator, gboolean extract_stray_words);
void kv_scanner_deinit(KVScanner *self);

static inline void
kv_scanner_input(KVScanner *self, const gchar *input)
{
  self->input = input;
  self->input_pos = 0;
  if (self->stray_words)
    g_string_truncate(self->stray_words, 0);
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

static inline const gchar *
kv_scanner_get_stray_words(KVScanner *self)
{
  return self->stray_words ? self->stray_words->str : NULL;
}

static inline void
kv_scanner_set_transform_value(KVScanner *self, KVTransformValueFunc transform_value)
{
  self->transform_value = transform_value;
}

static inline void
kv_scanner_set_extract_annotation_func(KVScanner *self, KVExtractAnnotationFunc extract_annotation)
{
  self->extract_annotation = extract_annotation;
}

static inline void
kv_scanner_set_valid_key_character_func(KVScanner *self, KVIsValidKeyCharFunc is_valid_key_character)
{
  self->is_valid_key_character = is_valid_key_character;
}

static inline void
kv_scanner_set_stop_character(KVScanner *self, gchar stop_char)
{
  self->stop_char = stop_char;
}

gboolean kv_scanner_scan_next(KVScanner *self);
KVScanner *kv_scanner_new(gchar value_separator, const gchar *pair_separator, gboolean extract_stray_words);
void kv_scanner_free(KVScanner *self);

#endif
