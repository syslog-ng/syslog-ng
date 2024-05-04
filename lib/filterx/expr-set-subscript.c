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
#include "filterx/filterx-eval.h"
#include "scratch-buffers.h"

typedef struct _FilterXSetSubscript
{
  FilterXExpr super;
  FilterXExpr *object;
  FilterXExpr *key;
  FilterXExpr *new_value;
} FilterXSetSubscript;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXSetSubscript *self = (FilterXSetSubscript *) s;
  FilterXObject *result = NULL;
  FilterXObject *new_value = NULL, *key = NULL, *object = NULL;

  object = filterx_expr_eval_typed(self->object);
  if (!object)
    return NULL;

  if (self->key)
    {
      key = filterx_expr_eval_typed(self->key);
      if (!key)
        goto exit;
    }
  else
    {
      /* append */
      key = NULL;
    }

  if (object->readonly)
    {
      filterx_eval_push_error("Object set-subscript failed, object is readonly", s, key);
      goto exit;
    }

  new_value = filterx_expr_eval_typed(self->new_value);
  if (!new_value)
    goto exit;

  FilterXObject *cloned = filterx_object_clone(new_value);
  filterx_object_unref(new_value);

  if (!filterx_object_set_subscript(object, key, &cloned))
    {
      filterx_eval_push_error("Object set-subscript failed", s, key);
      filterx_object_unref(cloned);
    }
  else
    {
      result = cloned;
    }

exit:
  filterx_object_unref(key);
  filterx_object_unref(object);
  return result;
}

static void
_free(FilterXExpr *s)
{
  FilterXSetSubscript *self = (FilterXSetSubscript *) s;

  filterx_expr_unref(self->key);
  filterx_expr_unref(self->object);
  filterx_expr_unref(self->new_value);
  filterx_expr_free_method(s);
}

FilterXExpr *
filterx_set_subscript_new(FilterXExpr *object, FilterXExpr *key, FilterXExpr *new_value)
{
  FilterXSetSubscript *self = g_new0(FilterXSetSubscript, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->object = object;
  self->key = key;
  self->new_value = new_value;
  self->super.ignore_falsy_result = TRUE;
  return &self->super;
}
