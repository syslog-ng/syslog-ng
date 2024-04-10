/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "expr-list.h"
#include "object-list-interface.h"
#include "object-primitive.h"
#include "scratch-buffers.h"

typedef struct _FilterXListExpr
{
  FilterXExpr super;
  FilterXExpr *fillable;
  GList *values;
} FilterXListExpr;

static gboolean
_eval_value(FilterXListExpr *self, FilterXObject *fillable, FilterXExpr *expr)
{
  FilterXObject *value = filterx_expr_eval_typed(expr);
  if (!value)
    return FALSE;

  FilterXObject *cloned_value = filterx_object_clone(value);
  filterx_object_unref(value);

  gboolean success = filterx_list_append(fillable, cloned_value);
  filterx_object_unref(cloned_value);
  return success;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXListExpr *self = (FilterXListExpr *) s;

  FilterXObject *fillable = filterx_expr_eval_typed(self->fillable);
  if (!fillable)
    return NULL;

  if (!filterx_object_is_type(fillable, &FILTERX_TYPE_NAME(list)))
    goto fail;

  for (GList *l = self->values; l; l = l->next)
    {
      if (!_eval_value(self, fillable, l->data))
        goto fail;
    }

  return fillable;

fail:
  filterx_object_unref(fillable);
  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXListExpr *self = (FilterXListExpr *) s;

  g_list_free_full(self->values, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_unref(self->fillable);
  filterx_expr_free_method(s);
}

FilterXExpr *
filterx_list_expr_new(FilterXExpr *fillable, GList *values)
{
  FilterXListExpr *self = g_new0(FilterXListExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->fillable = filterx_expr_ref(fillable);
  self->values = values;
  return &self->super;
}

static FilterXObject *
_inner_eval(FilterXExpr *s)
{
  FilterXListExpr *self = (FilterXListExpr *) s;

  FilterXObject *root_fillable = filterx_expr_eval_typed(self->fillable);
  if (!root_fillable)
    return NULL;

  FilterXObject *fillable = filterx_object_create_list(root_fillable);
  filterx_object_unref(root_fillable);
  if (!fillable)
    return NULL;

  if (!filterx_object_is_type(fillable, &FILTERX_TYPE_NAME(list)))
    goto fail;

  for (GList *l = self->values; l; l = l->next)
    {
      if (!_eval_value(self, fillable, l->data))
        goto fail;
    }

  return fillable;

fail:
  filterx_object_unref(fillable);
  return NULL;
}

FilterXExpr *
filterx_list_expr_inner_new(FilterXExpr *fillable, GList *values)
{
  FilterXListExpr *self = g_new0(FilterXListExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _inner_eval;
  self->super.free_fn = _free;
  self->fillable = filterx_expr_ref(fillable);
  self->values = values;
  return &self->super;
}
