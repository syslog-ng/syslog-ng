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
#include "filterx/expr-boolalg.h"
#include "filterx/object-primitive.h"

static FilterXObject *
_eval_not(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  FilterXObject *result = filterx_expr_eval(self->operand);
  if (!result)
    return NULL;

  gboolean truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (truthy)
    return filterx_boolean_new(FALSE);
  else
    return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_unary_not_new(FilterXExpr *operand)
{
  FilterXUnaryOp *self = g_new0(FilterXUnaryOp, 1);

  filterx_unary_op_init_instance(self, operand);
  self->super.eval = _eval_not;
  return &self->super;
}

static FilterXObject *
_eval_and(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  /* we only evaluate our rhs if lhs is true, e.g.  we do fast circuit
   * evaluation just like most languages */

  FilterXObject *result = filterx_expr_eval(self->lhs);
  if (!result)
    return NULL;

  gboolean lhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (!lhs_truthy)
    return filterx_boolean_new(FALSE);

  result = filterx_expr_eval(self->rhs);
  if (!result)
    return NULL;

  gboolean rhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (!rhs_truthy)
    return filterx_boolean_new(FALSE);

  return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_binary_and_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval_and;
  return &self->super;
}

static FilterXObject *
_eval_or(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  /* we only evaluate our rhs if lhs is false, e.g.  we do fast circuit
   * evaluation just like most languages */

  FilterXObject *result = filterx_expr_eval(self->lhs);
  if (!result)
    return NULL;

  gboolean lhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (lhs_truthy)
    return filterx_boolean_new(TRUE);

  result = filterx_expr_eval(self->rhs);
  if (!result)
    return NULL;

  gboolean rhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (rhs_truthy)
    return filterx_boolean_new(TRUE);

  return filterx_boolean_new(FALSE);
}

FilterXExpr *
filterx_binary_or_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval_or;
  return &self->super;
}
