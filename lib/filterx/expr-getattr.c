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
#include "filterx/expr-getattr.h"

typedef struct _FilterXGetAttr
{
  FilterXExpr super;
  FilterXExpr *operand;
  gchar *attr_name;
} FilterXGetAttr;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXGetAttr *self = (FilterXGetAttr *) s;

  FilterXObject *variable = filterx_expr_eval_typed(self->operand);

  if (!variable)
    return NULL;

  FilterXObject *attr = filterx_object_getattr(variable, self->attr_name);
  if (!attr)
    goto exit;
exit:
  filterx_object_unref(variable);
  return attr;
}

static void
_free(FilterXExpr *s)
{
  FilterXGetAttr *self = (FilterXGetAttr *) s;
  g_free(self->attr_name);
  filterx_expr_unref(self->operand);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_getattr_new(FilterXExpr *operand, const gchar *attr_name)
{
  FilterXGetAttr *self = g_new0(FilterXGetAttr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->operand = operand;
  self->attr_name = g_strdup(attr_name);
  return &self->super;
}
