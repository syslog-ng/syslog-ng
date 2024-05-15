/*
 * Copyright (c) 2024 shifter
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

#ifndef FILTERX_FUNC_PARSE_CSV_H_INCLUDED
#define FILTERX_FUNC_PARSE_CSV_H_INCLUDED

#include "plugin.h"
#include "filterx/expr-function.h"

#define FILTERX_FUNC_PARSE_CSV_ARG_NAME_COLUMNS "columns"
#define FILTERX_FUNC_PARSE_CSV_ARG_NAME_DELIMITERS "delimiters"
#define FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT "dialect"
#define FILTERX_FUNC_PARSE_CSV_ARG_NAME_STRIP_WHITESPACES "strip_whitespaces"
#define FILTERX_FUNC_PARSE_CSV_ARG_NAME_GREEDY "greedy"
#define FILTERX_FUNC_PARSE_CSV_USAGE "Usage: parse_csv(msg_str [" \
    FILTERX_FUNC_PARSE_CSV_ARG_NAME_COLUMNS"=json_array, " \
    FILTERX_FUNC_PARSE_CSV_ARG_NAME_DELIMITERS"=string, " \
    FILTERX_FUNC_PARSE_CSV_ARG_NAME_DIALECT"=string, " \
    FILTERX_FUNC_PARSE_CSV_ARG_NAME_STRIP_WHITESPACES"=boolean, " \
    FILTERX_FUNC_PARSE_CSV_ARG_NAME_GREEDY"=boolean])"

FilterXExpr *filterx_function_parse_csv_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error);
gpointer filterx_function_construct_parse_csv(Plugin *self);

#endif
