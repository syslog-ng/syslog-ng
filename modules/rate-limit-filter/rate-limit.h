/*
 * Copyright (c) 2021 One Identity
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

#ifndef RATE_LIMIT_H_INCLUDED
#define RATE_LIMIT_H_INCLUDED

#include "filter/filter-expr.h"

FilterExprNode *rate_limit_new(void);

void rate_limit_set_key_template(FilterExprNode *s, LogTemplate *template);
void rate_limit_set_key(FilterExprNode *s, NVHandle key_handle);
void rate_limit_set_rate(FilterExprNode *s, gint rate);

#endif
