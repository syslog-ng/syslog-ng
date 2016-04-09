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

typedef struct _KVScanner KVScanner;
struct _KVScanner
{
  const gchar *input;
  gsize input_pos;
  gsize input_len;
  GString *key;
  GString *value;
  GString *decoded_value;
  gboolean value_was_quoted;
  gchar value_separator;
  gchar quote_char;
  gint quote_state;
  gint next_quote_state;
  gboolean (*parse_value)(KVScanner *self);
  void (*free_fn)(KVScanner *self);
};

void kv_scanner_set_value_separator(KVScanner *self, gchar value_separator);
void kv_scanner_input(KVScanner *self, const gchar *input);
gboolean kv_scanner_scan_next(KVScanner *self);
const gchar *kv_scanner_get_current_key(KVScanner *self);
const gchar *kv_scanner_get_current_value(KVScanner *self);
KVScanner *kv_scanner_clone(KVScanner *self);
void kv_scanner_free_method(KVScanner *self);
void kv_scanner_init(KVScanner *self);

KVScanner *kv_scanner_new(void);
void kv_scanner_free(KVScanner *self);

#endif
