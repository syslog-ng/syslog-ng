/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef CSVPARSER_H_INCLUDED
#define CSVPARSER_H_INCLUDED

#include "parser/parser-expr.h"

#define CSV_PARSER_ESCAPE_NONE        0x0001
#define CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define CSV_PARSER_ESCAPE_MASK        0x0007
#define CSV_PARSER_STRIP_WHITESPACE   0x0008
#define CSV_PARSER_GREEDY             0x0010
#define CSV_PARSER_DROP_INVALID       0x0020

#define CSV_PARSER_FLAGS_DEFAULT  ((CSV_PARSER_STRIP_WHITESPACE) | (CSV_PARSER_ESCAPE_NONE))

void csv_parser_set_columns(LogParser *s, GList *fields);
void csv_parser_set_flags(LogParser *s, guint32 flags);
void csv_parser_set_delimiters(LogParser *s, const gchar *delimiters);
void csv_parser_set_string_delimiters(LogParser *s, GList *string_delimiters);
void csv_parser_set_quotes(LogParser *s, const gchar *quotes);
void csv_parser_set_quote_pairs(LogParser *s, const gchar *quote_pairs);
void csv_parser_set_null_value(LogParser *s, const gchar *null_value);
LogParser *csv_parser_new(GlobalConfig *cfg);
guint32 csv_parser_lookup_flag(const gchar *flag);
guint32 csv_parser_normalize_escape_flags(LogParser *s, guint32 new_flag);

guint32 csv_parser_get_flags(LogParser *s);

#endif
