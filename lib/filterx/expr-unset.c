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

#include "filterx/expr-unset.h"
#include "filterx/object-primitive.h"

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  if (!filterx_expr_unset(self->operand))
    return NULL;
  return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_unset_new(FilterXExpr *expr)
{
  FilterXUnaryOp *self = g_new0(FilterXUnaryOp, 1);
  filterx_unary_op_init_instance(self, expr);
  self->super.eval = _eval;
  return &self->super;
}
