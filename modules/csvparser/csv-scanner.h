/*
 * Copyright (c) 2002-2015 BalaBit
 * Copyright (c) 1998-2015 Balázs Scheidler
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


#ifndef CSVSCANNER_H_INCLUDED
#define CSVSCANNER_H_INCLUDED

#include "syslog-ng.h"

#define CSV_PARSER_ESCAPE_NONE        0x0001
#define CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define CSV_PARSER_ESCAPE_MASK        0x0007
#define CSV_PARSER_STRIP_WHITESPACE   0x0008
#define CSV_PARSER_GREEDY             0x0010
#define CSV_PARSER_DROP_INVALID       0x0020

typedef struct _CSVScannerOptions
{
  GList *columns;
  gchar *delimiters;
  gchar *quotes_start;
  gchar *quotes_end;
  gchar *null_value;
  GList *string_delimiters;
  guint32 flags;
} CSVScannerOptions;

void csv_scanner_options_clean(CSVScannerOptions *options);
void csv_scanner_options_copy(CSVScannerOptions *dst, CSVScannerOptions *src);

guint32 csv_scanner_options_normalize_escape_flags(CSVScannerOptions *options, guint32 new_flag);
void csv_scanner_options_set_flags(CSVScannerOptions *options, guint32 flags);
guint32 csv_scanner_options_get_flags(CSVScannerOptions *options);
void csv_scanner_options_set_columns(CSVScannerOptions *options, GList *columns);
void csv_scanner_options_set_delimiters(CSVScannerOptions *options, const gchar *delimiters);
void csv_scanner_options_set_string_delimiters(CSVScannerOptions *options, GList *string_delimiters);
void csv_scanner_options_set_quotes_start_and_end(CSVScannerOptions *options, const gchar *quotes_start, const gchar *quotes_end);
void csv_scanner_options_set_quotes(CSVScannerOptions *options, const gchar *quotes);
void csv_scanner_options_set_quote_pairs(CSVScannerOptions *options, const gchar *quote_pairs);
void csv_scanner_options_set_null_value(CSVScannerOptions *options, const gchar *null_value);

typedef struct _CSVScanner
{
  CSVScannerOptions *options;
  GList *current_column;
  const gchar *src;
  GString *current_value;
  gchar current_quote;
} CSVScanner;

const gchar *csv_scanner_get_current_name(CSVScanner *pstate);
const gchar *csv_scanner_get_current_value(CSVScanner *pstate);
gint csv_scanner_get_current_value_len(CSVScanner *self);
gboolean csv_scanner_scan_next(CSVScanner *pstate);
gboolean csv_scanner_is_scan_finished(CSVScanner *pstate);

void csv_scanner_input(CSVScanner *pstate, const gchar *input);
gboolean csv_scanner_parse_input(CSVScanner *pstate);
void csv_scanner_state_init(CSVScanner *pstate, CSVScannerOptions *options);
void csv_scanner_state_clean(CSVScanner *pstate);

#endif
