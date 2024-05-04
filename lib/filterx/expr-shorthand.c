/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx/expr-shorthand.h"

typedef struct _FilterXShorthand
{
  FilterXExpr super;
  GList *exprs;
} FilterXShorthand;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXShorthand *self = (FilterXShorthand *) s;
  FilterXObject *result = NULL;

  filterx_expr_list_eval(self->exprs, &result);
  return result;
}

static void
_free(FilterXExpr *s)
{
  FilterXShorthand *self = (FilterXShorthand *) s;

  g_list_free_full(self->exprs, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_free_method(s);
}

/* Takes reference of expr */
void
filterx_shorthand_add(FilterXExpr *s, FilterXExpr *expr)
{
  FilterXShorthand *self = (FilterXShorthand *) s;

  self->exprs = g_list_append(self->exprs, expr);
}

FilterXExpr *
filterx_shorthand_new(void)
{
  FilterXShorthand *self = g_new0(FilterXShorthand, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;

  return &self->super;
}

