/*
 * Copyright (c) 2021 One Identity
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef FILTER_THROTTLE_H_INCLUDED
#define FILTER_THROTTLE_H_INCLUDED

#include "filter-expr.h"

FilterExprNode *filter_throttle_new(void);

void filter_throttle_set_key_template(FilterExprNode *s, LogTemplate *template);
void filter_throttle_set_key(FilterExprNode *s, NVHandle key_handle);
void filter_throttle_set_rate(FilterExprNode *s, gint rate);

#endif