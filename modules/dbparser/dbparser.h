/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef DBPARSER_H_INCLUDED
#define DBPARSER_H_INCLUDED

#include "parser/parser-expr.h"
#include "patterndb.h"

#define PATH_PATTERNDB_FILE     PATH_LOCALSTATEDIR "/patterndb.xml"
#define PATH_XSDDIR             PATH_DATADIR "/xsd"

typedef struct _LogDBParser LogDBParser;

void log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file);
void log_db_parser_set_inject_mode(LogDBParser *self, const gchar *inject_mode);
LogParser *log_db_parser_new(void);

void log_pattern_database_init(void);

#endif
