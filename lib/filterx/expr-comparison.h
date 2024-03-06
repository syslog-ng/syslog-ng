/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef FILTERX_COMPARISON_H_INCLUDED
#define FILTERX_COMPARISON_H_INCLUDED

#include "filterx/filterx-expr.h"

#define FCMPX_EQ                   0x0001
#define FCMPX_LT                   0x0002
#define FCMPX_GT                   0x0004
#define FCMPX_NE                   0x0006 // (FCMPX_LT + FCMPX_GT)
#define FCMPX_TYPE_AWARE           0x0010
#define FCMPX_STRING_BASED         0x0020
#define FCMPX_NUM_BASED            0x0040
#define FCMPX_TYPE_AND_VALUE_BASED 0x0080

#define FCMPX_OP_MASK      0x0007
#define FCMPX_MODE_MASK    0x00F0

FilterXExpr *filterx_comparison_new(FilterXExpr *lhs, FilterXExpr *rhs, gint operator);


#endif
