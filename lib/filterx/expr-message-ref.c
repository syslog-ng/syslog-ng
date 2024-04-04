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
#include "filterx/expr-message-ref.h"
#include "filterx/object-message-value.h"
#include "filterx/object-primitive.h"
#include "filterx/filterx-scope.h"
#include "logmsg/logmsg.h"

struct _FilterXMessageRefExpr
{
  FilterXExpr super;
  NVHandle handle;
};

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();
  LogMessage *msg = context->msgs[0];
  FilterXObject *msg_ref;

  if (filterx_scope_is_message_ref_unset(context->scope, self->handle))
    return NULL;

  msg_ref = filterx_scope_lookup_message_ref(context->scope, self->handle);
  if (msg_ref)
    return msg_ref;

  gssize value_len;
  LogMessageValueType t;
  const gchar *value = log_msg_get_value_if_set_with_type(msg, self->handle, &value_len, &t);
  if (!value)
    return NULL;

  msg_ref = filterx_message_value_new_borrowed(value, value_len, t);
  filterx_scope_register_message_ref(context->scope, self->handle, msg_ref);
  return msg_ref;
}

static void
_update_repr(FilterXExpr *s, FilterXObject *new_repr)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();

  filterx_scope_register_message_ref(context->scope, self->handle, new_repr);
}

static gboolean
_assign(FilterXExpr *s, FilterXObject *new_value)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXScope *scope = filterx_eval_get_scope();

  /* this only clones mutable objects */
  new_value = filterx_object_clone(new_value);

  filterx_scope_register_message_ref(scope, self->handle, new_value);
  new_value->assigned = TRUE;

  filterx_object_unref(new_value);
  return TRUE;
}

static void
_free(FilterXExpr *s)
{
//  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
}

FilterXExpr *
filterx_message_ref_expr_new(NVHandle handle)
{
  FilterXMessageRefExpr *self = g_new0(FilterXMessageRefExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super._update_repr = _update_repr;
  self->super.assign = _assign;
  self->super.free_fn = _free;
  self->handle = handle;
  return &self->super;
}

FilterXObject *
_isset_eval(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  FilterXObject *message_ref = filterx_expr_eval(self->operand);
  if (!message_ref)
    return filterx_boolean_new(FALSE);

  filterx_object_unref(message_ref);
  return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_message_ref_isset_expr_new(FilterXMessageRefExpr *message_ref_expr)
{
  FilterXUnaryOp *self = g_new0(FilterXUnaryOp, 1);
  filterx_unary_op_init_instance(self, &message_ref_expr->super);
  self->super.eval = _isset_eval;
  return &self->super;
}

FilterXObject *
_unset_eval(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  FilterXObject *message_ref = filterx_expr_eval(self->operand);
  if (!message_ref)
    return filterx_boolean_new(FALSE);

  filterx_object_unref(message_ref);

  FilterXScope *scope = filterx_eval_get_scope();
  FilterXMessageRefExpr *message_ref_expr = (FilterXMessageRefExpr *) self->operand;
  filterx_scope_unset_message_ref(scope, message_ref_expr->handle);

  return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_message_ref_unset_expr_new(FilterXMessageRefExpr *message_ref_expr)
{
  FilterXUnaryOp *self = g_new0(FilterXUnaryOp, 1);
  filterx_unary_op_init_instance(self, &message_ref_expr->super);
  self->super.eval = _unset_eval;
  return &self->super;
}
