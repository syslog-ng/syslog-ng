/*
 * Copyright (c) 2002-2015 Balabit
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

#ifndef CSVPARSER_H_INCLUDED
#define CSVPARSER_H_INCLUDED

#include "parser/parser-expr.h"
#include "scanner/csv-scanner/csv-scanner.h"

CSVScannerOptions *csv_parser_get_scanner_options(LogParser *s);
gboolean csv_parser_set_flags(LogParser *s, guint32 flags);
void csv_parser_set_drop_invalid(LogParser *s, gboolean drop_invalid);
void csv_parser_set_prefix(LogParser *s, const gchar *prefix);
LogParser *csv_parser_new(GlobalConfig *cfg);

guint32 csv_parser_lookup_flag(const gchar *flag);
gint csv_parser_lookup_dialect(const gchar *flag);


#endif
