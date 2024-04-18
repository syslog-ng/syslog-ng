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
#include "filterx/expr-variable.h"
#include "filterx/object-message-value.h"
#include "filterx/filterx-scope.h"
#include "logmsg/logmsg.h"

typedef struct _FilterXVariableExpr
{
  FilterXExpr super;
  NVHandle handle;
  gboolean declared;
} FilterXVariableExpr;

static FilterXObject *
_pull_variable_from_message(FilterXVariableExpr *self, FilterXEvalContext *context, LogMessage *msg)
{
  gssize value_len;
  LogMessageValueType t;
  const gchar *value = log_msg_get_value_if_set_with_type(msg, self->handle, &value_len, &t);
  if (!value)
    return NULL;

  FilterXObject *msg_ref = filterx_message_value_new_borrowed(value, value_len, t);
  filterx_scope_register_variable(context->scope, self->handle, msg_ref);
  return msg_ref;
}

/* NOTE: unset on a variable that only exists in the LogMessage, without making the message writable */
static void
_whiteout_variable(FilterXVariableExpr *self, FilterXEvalContext *context)
{
  filterx_scope_register_variable(context->scope, self->handle, NULL);
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();
  FilterXVariable *variable;

  variable = filterx_scope_lookup_variable(context->scope, self->handle);
  if (variable)
    return filterx_variable_get_value(variable);

  if (!filterx_variable_handle_is_floating(self->handle))
    return _pull_variable_from_message(self, context, context->msgs[0]);
  return NULL;
}

static void
_update_repr(FilterXExpr *s, FilterXObject *new_repr)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;
  FilterXScope *scope = filterx_eval_get_scope();
  FilterXVariable *variable = filterx_scope_lookup_variable(scope, self->handle);

  g_assert(variable != NULL);
  filterx_variable_set_value(variable, new_repr);
}

static gboolean
_assign(FilterXExpr *s, FilterXObject *new_value)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;
  FilterXScope *scope = filterx_eval_get_scope();
  FilterXVariable *variable = filterx_scope_lookup_variable(scope, self->handle);

  if (!variable)
    {
      /* NOTE: we pass NULL as initial_value to make sure the new variable
       * is considered changed due to the assignment */

      variable = filterx_scope_register_variable(scope, self->handle, NULL);
      if (self->declared)
        filterx_variable_mark_declared(variable);
    }

  /* this only clones mutable objects */
  new_value = filterx_object_clone(new_value);
  filterx_variable_set_value(variable, new_value);
  filterx_object_unref(new_value);
  return TRUE;
}

static gboolean
_isset(FilterXExpr *s)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;
  FilterXScope *scope = filterx_eval_get_scope();

  FilterXVariable *variable = filterx_scope_lookup_variable(scope, self->handle);
  if (variable)
    return filterx_variable_is_set(variable);

  FilterXEvalContext *context = filterx_eval_get_context();
  LogMessage *msg = context->msgs[0];
  return log_msg_is_value_set(msg, self->handle);
}

static gboolean
_unset(FilterXExpr *s)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();

  FilterXVariable *variable = filterx_scope_lookup_variable(context->scope, self->handle);
  if (variable)
    {
      filterx_variable_unset_value(variable);
      return TRUE;
    }

  LogMessage *msg = context->msgs[0];
  if (log_msg_is_value_set(msg, self->handle))
    _whiteout_variable(self, context);

  return TRUE;
}

static FilterXExpr *
filterx_variable_expr_new(const gchar *name, FilterXVariableType type)
{
  FilterXVariableExpr *self = g_new0(FilterXVariableExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super._update_repr = _update_repr;
  self->super.assign = _assign;
  self->super.isset = _isset;
  self->super.unset = _unset;
  self->handle = filterx_scope_map_variable_to_handle(name, type);
  return &self->super;
}

FilterXExpr *
filterx_msg_variable_expr_new(const gchar *name)
{
  return filterx_variable_expr_new(name, FX_VAR_MESSAGE);
}

FilterXExpr *
filterx_floating_variable_expr_new(const gchar *name)
{
  return filterx_variable_expr_new(name, FX_VAR_FLOATING);
}

void
filterx_variable_expr_declare(FilterXExpr *s)
{
  FilterXVariableExpr *self = (FilterXVariableExpr *) s;

  g_assert(s->eval == _eval);
  self->declared = TRUE;
}