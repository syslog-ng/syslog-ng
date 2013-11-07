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

#define LOG_CSV_PARSER_ESCAPE_NONE        0x0001
#define LOG_CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define LOG_CSV_PARSER_ESCAPE_MASK        0x0007
#define LOG_CSV_PARSER_STRIP_WHITESPACE   0x0008
#define LOG_CSV_PARSER_GREEDY             0x0010
#define LOG_CSV_PARSER_DROP_INVALID       0x0020

#define LOG_CSV_PARSER_FLAGS_DEFAULT  ((LOG_CSV_PARSER_STRIP_WHITESPACE) | (LOG_CSV_PARSER_ESCAPE_NONE))

void log_csv_parser_set_flags(LogColumnParser *s, guint32 flags);
void log_csv_parser_set_delimiters(LogColumnParser *s, const gchar *delimiters);
void log_csv_parser_set_quotes(LogColumnParser *s, const gchar *quotes);
void log_csv_parser_set_quote_pairs(LogColumnParser *s, const gchar *quote_pairs);
void log_csv_parser_set_null_value(LogColumnParser *s, const gchar *null_value);
LogColumnParser *log_csv_parser_new(void);
guint32 log_csv_parser_lookup_flag(const gchar *flag);
guint32 log_csv_parser_normalize_escape_flags(LogColumnParser *s, guint32 new_flag);

guint32 log_csv_parser_get_flags(LogColumnParser *s);

#endif
