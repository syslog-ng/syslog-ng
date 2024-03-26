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
#include "filterx/expr-merge.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"

static gboolean
_merge_dicts_iter_func(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  FilterXObject *lhs = (FilterXObject *) user_data;
  FilterXObject *cloned_value = filterx_object_clone(value);

  gboolean success = filterx_object_set_subscript(lhs, key, cloned_value);

  filterx_object_unref(cloned_value);
  return success;
}

static FilterXObject *
_merge_dicts(FilterXObject *lhs, FilterXObject *rhs)
{
  if (!filterx_dict_iter(rhs, _merge_dicts_iter_func, (gpointer) lhs))
    {
      filterx_object_unref(lhs);
      filterx_object_unref(rhs);
      return NULL;
    }

  filterx_object_unref(rhs);
  return lhs;
}

static FilterXObject *
_merge_lists(FilterXObject *lhs, FilterXObject *rhs)
{
  guint64 rhs_len = filterx_list_len(rhs);
  for (guint64 i = 0; i < rhs_len; i++)
    {
      FilterXObject *elem = filterx_list_get_subscript(rhs, i);
      FilterXObject *cloned_elem = filterx_object_clone(elem);
      filterx_object_unref(elem);

      gboolean success = filterx_list_append(lhs, cloned_elem);
      filterx_object_unref(cloned_elem);

      if (!success)
        {
          filterx_object_unref(lhs);
          filterx_object_unref(rhs);
          return NULL;
        }
    }

  filterx_object_unref(rhs);
  return lhs;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  FilterXObject *lhs = NULL, *rhs = NULL;

  lhs = filterx_expr_eval(self->lhs);
  if (!lhs)
    goto error;

  rhs = filterx_expr_eval(self->rhs);
  if (!rhs)
    goto error;

  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(dict)))
    {
      if (!filterx_object_is_type(rhs, &FILTERX_TYPE_NAME(dict)))
        goto error;
      return _merge_dicts(lhs, rhs);
    }

  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(list)))
    {
      if (!filterx_object_is_type(rhs, &FILTERX_TYPE_NAME(list)))
        goto error;
      return _merge_lists(lhs, rhs);
    }

error:
  filterx_object_unref(lhs);
  filterx_object_unref(rhs);
  return NULL;
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_merge_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval;
  return &self->super;
}
