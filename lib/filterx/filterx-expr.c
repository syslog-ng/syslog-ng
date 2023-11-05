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

#include "filterx/filterx-expr.h"
#include "messages.h"

void
filterx_expr_free_method(FilterXExpr *self)
{
  ;
}

void
filterx_expr_init_instance(FilterXExpr *self)
{
  self->ref_cnt = 1;
  self->free_fn = filterx_expr_free_method;
}

FilterXExpr *
filterx_expr_new(void)
{
  FilterXExpr *self = g_new0(FilterXExpr, 1);
  filterx_expr_init_instance(self);
  return self;
}

FilterXExpr *
filterx_expr_ref(FilterXExpr *self)
{
  self->ref_cnt++;
  return self;
}

void
filterx_expr_unref(FilterXExpr *self)
{
  if (--self->ref_cnt == 0)
    {
      self->free_fn(self);
      g_free(self);
    }
}

void
filterx_unary_op_free_method(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  filterx_expr_unref(self->operand);
}

void
filterx_unary_op_init_instance(FilterXUnaryOp *self, FilterXExpr *operand)
{
  filterx_expr_init_instance(&self->super);
  self->super.free_fn = filterx_unary_op_free_method;
  self->operand = operand;
}

void
filterx_binary_op_free_method(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  filterx_expr_unref(self->lhs);
  filterx_expr_unref(self->rhs);
}

void
filterx_binary_op_init_instance(FilterXBinaryOp *self, FilterXExpr *lhs, FilterXExpr *rhs)
{
  filterx_expr_init_instance(&self->super);
  self->super.free_fn = filterx_binary_op_free_method;
  self->lhs = lhs;
  self->rhs = rhs;
}
