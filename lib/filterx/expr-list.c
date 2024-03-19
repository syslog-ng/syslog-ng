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
#include "expr-list.h"
#include "object-json.h"
#include "object-primitive.h"
#include "scratch-buffers.h"
// #include "compat/json.h"

typedef struct _FilterXListExpr
{
  FilterXExpr super;
  GList *values;
} FilterXListExpr;

static gboolean
_eval_value(FilterXListExpr *self, FilterXObject *object, gint i, FilterXExpr *expr)
{
  FilterXObject *index = filterx_integer_new(i);
  FilterXObject *value = filterx_expr_eval_typed(expr);
  if (!value)
    return FALSE;

  FilterXObject *cloned_value = filterx_object_clone(value);
  filterx_object_unref(value);

  gboolean success = filterx_object_set_subscript(object, index, cloned_value);
  filterx_object_unref(cloned_value);
  return success;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXListExpr *self = (FilterXListExpr *) s;
  FilterXObject *object = filterx_json_array_new_empty();

  gint index = 0;
  for (GList *l = self->values; l; l = l->next)
    {
      if (!_eval_value(self, object, index, l->data))
        goto fail;
      index += 1;
    }
  return object;
fail:
  filterx_object_unref(object);
  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXListExpr *self = (FilterXListExpr *) s;

  g_list_free_full(self->values, (GDestroyNotify) filterx_expr_unref);
}

FilterXExpr *
filterx_list_expr_new(GList *values)
{
  FilterXListExpr *self = g_new0(FilterXListExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->values = values;
  return &self->super;
}
