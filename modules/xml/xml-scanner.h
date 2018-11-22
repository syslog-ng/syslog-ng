/*
 * Copyright (c) 2018 Balabit
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
#ifndef XML_SCANNER_H_INCLUDED
#define XML_SCANNER_H_INCLUDED

#include "xml.h"
#include "syslog-ng.h"

typedef struct
{
  LogMessage *msg;
  GString *key;
  XMLParser *parser;
} InserterState;

typedef struct
{
  GMarkupParseContext *xml_ctx;
  InserterState *state;
  gboolean pop_next_time;
  gboolean matchstring_shouldreverse;
  GPtrArray *exclude_patterns;
} XMLScanner;

// see Inserterstate->parser elements
// they could be become internal options initialized by parser options and defaults
void xml_scanner_init(XMLScanner *self, InserterState *state);
void xml_scanner_deinit(XMLScanner *self);

void xml_scanner_parse(XMLScanner *self, const gchar *input, gsize input_len, GError **error);
void xml_scanner_end_parse(XMLScanner *self, GError **error);


gboolean joker_or_wildcard(GList *patterns);
#endif
