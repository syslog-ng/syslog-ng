/*
 * Copyright (c) 2011-2012 Balabit
 * Copyright (c) 2011-2012 Gergely Nagy <algernon@balabit.hu>
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

#ifndef JSONPARSER_H_INCLUDED
#define JSONPARSER_H_INCLUDED

#include "parser/parser-expr.h"

void json_parser_set_extract_prefix(LogParser *s, const gchar *extract_prefix);
void json_parser_set_prefix(LogParser *p, const gchar *prefix);
void json_parser_set_marker(LogParser *p, const gchar *marker);
LogParser *json_parser_new(GlobalConfig *cfg);

#endif
