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
#include "filterx/expr-get-subscript.h"

typedef struct _FilterXGetSubscript
{
  FilterXExpr super;
  FilterXExpr *operand;
  FilterXExpr *index;
} FilterXGetSubscript;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXGetSubscript *self = (FilterXGetSubscript *) s;
  FilterXObject *result = NULL;

  FilterXObject *variable = filterx_expr_eval_typed(self->operand);
  if (!variable)
    return NULL;

  FilterXObject *index = filterx_expr_eval_typed(self->index);
  if (!index)
    goto exit;
  result = filterx_object_get_subscript(variable, index);
exit:
  filterx_object_unref(index);
  filterx_object_unref(variable);
  return result;
}

static void
_free(FilterXExpr *s)
{
  FilterXGetSubscript *self = (FilterXGetSubscript *) s;
  filterx_expr_unref(self->index);
  filterx_expr_unref(self->operand);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_get_subscript_new(FilterXExpr *operand, FilterXExpr *index)
{
  FilterXGetSubscript *self = g_new0(FilterXGetSubscript, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->operand = operand;
  self->index = index;
  return &self->super;
}
