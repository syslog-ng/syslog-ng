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
#include "filterx/expr-assign.h"
#include "filterx/object-primitive.h"
#include "scratch-buffers.h"

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  FilterXObject *value = filterx_expr_eval(self->rhs);

  if (!value)
    return NULL;

  if (!filterx_expr_assign(self->lhs, value))
    {
      filterx_object_unref(value);
      return NULL;
    }

  return value;
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_assign_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval;
  self->super.ignore_falsy_result = TRUE;
  return &self->super;
}
