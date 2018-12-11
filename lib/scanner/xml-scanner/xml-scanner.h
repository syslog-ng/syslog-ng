/*
 * Copyright (c) 2018 Balabit
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
#ifndef XML_SCANNER_H_INCLUDED
#define XML_SCANNER_H_INCLUDED

#include "syslog-ng.h"
#include "messages.h"

#include <string.h>

typedef struct _XMLScanner XMLScanner;

typedef void (*PushCurrentKeyValueCB)(const gchar *name, const gchar *value, gssize value_length, gpointer user_data);

typedef struct
{
  gboolean strip_whitespaces;
  GList *exclude_tags;
  gboolean matchstring_shouldreverse;
  GPtrArray *exclude_patterns;
} XMLScannerOptions;

typedef struct
{
  PushCurrentKeyValueCB push_function;
  gpointer user_data;
} PushCurrentKeyValue;

struct _XMLScanner
{
  GMarkupParseContext *xml_ctx;
  XMLScannerOptions *options;
  gboolean pop_next_time;
  GString *key;
  GString *text;
  GQueue *text_stack;
  gboolean (*start_element_cb) (XMLScanner *self, const gchar *element_name, const gchar **attribute_names,
                                const gchar **attribute_values, GError **error);
  void (*end_element_cb) (XMLScanner *self, const gchar *element_name, GError **error);
  void (*text_cb) (XMLScanner *self, const gchar *element_name,
                   const gchar *text, gsize text_len, GError **error);
  PushCurrentKeyValue push_key_value;
};

void xml_scanner_init(XMLScanner *self, XMLScannerOptions *options, PushCurrentKeyValueCB push_function,
                      gpointer user_data, gchar *key_prefix);
void xml_scanner_deinit(XMLScanner *self);
void xml_scanner_parse(XMLScanner *self, const gchar *input, gsize input_len, GError **error);

static inline void
xml_scanner_push_current_key_value(XMLScanner *self, const gchar *name, const gchar *value, gssize value_length)
{
  self->push_key_value.push_function(name, value, value_length, self->push_key_value.user_data);
}

gboolean xml_scanner_start_element_method(XMLScanner *self, const gchar *element_name, const gchar **attribute_names,
                                          const gchar **attribute_values, GError **error);
void xml_scanner_end_element_method(XMLScanner *self, const gchar *element_name, GError **error);
void xml_scanner_text_method(XMLScanner *self, const gchar *element_name,
                             const gchar *text, gsize text_len, GError **error);

void xml_scanner_options_set_and_compile_exclude_tags(XMLScannerOptions *self, GList *exclude_tags);
void xml_scanner_options_set_strip_whitespaces(XMLScannerOptions *self, gboolean setting);
void xml_scanner_options_copy(XMLScannerOptions *dest, XMLScannerOptions *source);

void xml_scanner_options_destroy(XMLScannerOptions *self);
void xml_scanner_options_defaults(XMLScannerOptions *self);

gboolean joker_or_wildcard(GList *patterns);
#endif
