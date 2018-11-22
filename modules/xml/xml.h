/*
 * Copyright (c) 2017 Balabit
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

#ifndef XML_H_INCLUDED
#define XML_H_INCLUDED

#include "parser/parser-expr.h"

typedef struct
{
  LogParser super;
  gchar *prefix;
  gboolean forward_invalid;
  GList *exclude_tags;
  gboolean strip_whitespaces;
} XMLParser;

LogParser *xml_parser_new(GlobalConfig *cfg);
LogPipe *xml_parser_clone(LogPipe *s);
void xml_parser_set_prefix(LogParser *s, const gchar *prefix);
void xml_parser_set_forward_invalid(LogParser *s, gboolean setting);
void xml_parser_set_exclude_tags(LogParser *s, GList *exclude_tags);
void xml_parser_set_strip_whitespaces(LogParser *s, gboolean setting);

#endif
