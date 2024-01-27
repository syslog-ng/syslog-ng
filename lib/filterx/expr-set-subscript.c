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
#include "filterx/expr-set-subscript.h"
#include "filterx/object-primitive.h"

typedef struct _FilterXSetSubscript
{
  FilterXExpr super;
  FilterXExpr *object;
  FilterXExpr *index;
  FilterXExpr *new_value;
} FilterXSetSubscript;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXSetSubscript *self = (FilterXSetSubscript *) s;
  FilterXObject *result = NULL;
  FilterXObject *new_value = NULL, *index = NULL, *object = NULL;

  object = filterx_expr_eval_typed(self->object);
  if (!object)
    return NULL;

  if (self->index)
    {
      index = filterx_expr_eval_typed(self->index);
      if (!index)
        goto exit;
    }
  else
    {
      /* append */
      index = NULL;
    }

  new_value = filterx_expr_eval_typed(self->new_value);
  if (!new_value)
    goto exit;

  if (!filterx_object_set_subscript(object, index, new_value))
    goto exit;
  result = filterx_object_ref(new_value);
exit:
  filterx_object_unref(new_value);
  filterx_object_unref(index);
  filterx_object_unref(object);
  return result;
}

static void
_free(FilterXExpr *s)
{
  FilterXSetSubscript *self = (FilterXSetSubscript *) s;

  filterx_expr_unref(self->index);
  filterx_expr_unref(self->object);
  filterx_expr_unref(self->new_value);
}

FilterXExpr *
filterx_set_subscript_new(FilterXExpr *object, FilterXExpr *index, FilterXExpr *new_value)
{
  FilterXSetSubscript *self = g_new0(FilterXSetSubscript, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->object = object;
  self->index = index;
  self->new_value = new_value;
  return &self->super;
}
