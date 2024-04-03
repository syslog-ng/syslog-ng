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

#include "filterx/expr-is-key-set.h"
#include "filterx/object-primitive.h"

typedef struct _FilterXIsKeySet
{
  FilterXExpr super;
  FilterXExpr *operand;
  FilterXExpr *key;
} FilterXIsKeySet;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXIsKeySet *self = (FilterXIsKeySet *) s;

  FilterXObject *variable = filterx_expr_eval_typed(self->operand);
  if (!variable)
    return NULL;

  FilterXObject *key = filterx_expr_eval_typed(self->key);
  if (!key)
    {
      filterx_object_unref(variable);
      return NULL;
    }

  gboolean result = filterx_object_is_key_set(variable, key);

  filterx_object_unref(key);
  filterx_object_unref(variable);
  return filterx_boolean_new(result);
}

static void
_free(FilterXExpr *s)
{
  FilterXIsKeySet *self = (FilterXIsKeySet *) s;
  filterx_expr_unref(self->key);
  filterx_expr_unref(self->operand);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_is_key_set_new(FilterXExpr *operand, FilterXExpr *key)
{
  FilterXIsKeySet *self = g_new0(FilterXIsKeySet, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->operand = operand;
  self->key = key;
  return &self->super;
}
