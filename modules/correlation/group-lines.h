/*
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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
#ifndef CORRELATION_GROUP_LINES_PARSER_H_INCLUDED
#define CORRELATION_GROUP_LINES_PARSER_H_INCLUDED

#include "stateful-parser.h"
#include "synthetic-message.h"
#include "multi-line/multi-line-factory.h"

MultiLineOptions *group_lines_get_multi_line_options(LogParser *s);
void group_lines_set_separator(LogParser *s, const gchar *separator);
LogParser *group_lines_new(GlobalConfig *cfg);

#endif
