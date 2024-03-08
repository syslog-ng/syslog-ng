/*
 * Copyright (c) 2023 shifter <shifter@axoflow.com>
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

#ifndef FILTERX_CONDITION_H_INCLUDED
#define FILTERX_CONDITION_H_INCLUDED

#include "filterx/filterx-expr.h"

typedef struct _FilterXConditional FilterXConditional;

struct _FilterXConditional
{
  FilterXExpr super;
  FilterXExpr *condition;
  GList *statements;
  FilterXConditional *false_branch;
};

#define FILTERX_CONDITIONAL_NO_CONDITION NULL

FilterXExpr *filterx_conditional_new_conditional_codeblock(FilterXExpr *condition, GList *stmts);
FilterXExpr *filterx_conditional_new_codeblock(GList *stmts);
FilterXExpr *filterx_conditional_add_false_branch(FilterXConditional *s, FilterXConditional *fail_branch);

#endif
