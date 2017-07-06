/*
 * Copyright (c) 2016 Balabit
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

#ifndef _SNG_PYTHON_LOGPARSER_H
#define _SNG_PYTHON_LOGPARSER_H

#include "python-module.h"
#include "parser/parser-expr.h"
#include "value-pairs/value-pairs.h"

LogParser *python_parser_new(GlobalConfig *cfg);
void python_parser_set_imports(LogParser *s, GList *imports);
void python_parser_set_class(LogParser *s, gchar *class_name);
void python_parser_set_option(LogParser  *s, gchar *key, gchar *value);

#endif
