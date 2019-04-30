/*
 * Copyright (c) 2015 BalaBit
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
#ifndef PATTERNDB_GROUPING_BY_PARSER_H_INCLUDED
#define PATTERNDB_GROUPING_BY_PARSER_H_INCLUDED

#include "stateful-parser.h"
#include "synthetic-message.h"
#include "filter/filter-expr.h"

void grouping_by_set_key_template(LogParser *s, LogTemplate *context_id);
void grouping_by_set_sort_key_template(LogParser *s, LogTemplate *sort_key);
void grouping_by_set_timeout(LogParser *s, gint timeout);
void grouping_by_set_scope(LogParser *s, CorrellationScope scope);
void grouping_by_set_synthetic_message(LogParser *s, SyntheticMessage *message);
void grouping_by_set_trigger_condition(LogParser *s, FilterExprNode *filter_expr);
void grouping_by_set_where_condition(LogParser *s, FilterExprNode *filter_expr);
void grouping_by_set_having_condition(LogParser *s, FilterExprNode *filter_expr);
gchar *grouping_by_format_persist_name(LogParser *s);
LogParser *grouping_by_new(GlobalConfig *cfg);
void grouping_by_global_init(void);

#endif
