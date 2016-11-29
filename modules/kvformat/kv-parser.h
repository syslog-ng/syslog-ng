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
 */

#ifndef KVPARSER_H_INCLUDED
#define KVPARSER_H_INCLUDED

#include "parser/parser-expr.h"
#include "kv-scanner.h"

/* base class */
typedef struct _KVParser
{
  LogParser super;
  gboolean allow_pair_separator_in_value;
  gchar value_separator;
  gchar *prefix;
  gsize prefix_len;
  GString *formatted_key;
  KVScanner *kv_scanner;
} KVParser;

void kv_parser_set_prefix(LogParser *p, const gchar *prefix);
void kv_parser_set_allow_pair_separator_in_value(LogParser *p, gboolean allow_pair_separator_in_value);
void kv_parser_set_value_separator(LogParser *p, gchar value_separator);
gboolean kv_parser_is_valid_separator_character(gchar c);

gboolean kv_parser_init_method(LogPipe *s);
gboolean kv_parser_deinit_method(LogPipe *s);
LogPipe *kv_parser_clone_method(KVParser *dst, KVParser *src);
KVParser *kv_parser_init_instance(GlobalConfig *cfg);
LogParser *kv_parser_new(GlobalConfig *cfg);

#endif
