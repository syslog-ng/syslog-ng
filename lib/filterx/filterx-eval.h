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
#ifndef FILTERX_EVAL_H_INCLUDED
#define FILTERX_EVAL_H_INCLUDED

#include "filterx/filterx-scope.h"
#include "filterx/filterx-expr.h"
#include "template/eval.h"

typedef struct _FilterXError
{
  const gchar *message;
  FilterXExpr *expr;
  FilterXObject *object;
} FilterXError;

typedef struct _FilterXEvalContext FilterXEvalContext;
struct _FilterXEvalContext
{
  LogMessage **msgs;
  gint num_msg;
  FilterXScope *scope;
  FilterXError error;
  LogTemplateEvalOptions *template_eval_options;
  FilterXEvalContext *previous_context;
};

FilterXEvalContext *filterx_eval_get_context(void);
FilterXScope *filterx_eval_get_scope(void);
void filterx_eval_push_error(const gchar *message, FilterXExpr *expr, FilterXObject *object);
void filterx_eval_set_context(FilterXEvalContext *context);
gboolean filterx_eval_exec_statements(FilterXEvalContext *context, GList *statements, LogMessage *msg);
void filterx_eval_sync_scope_and_message(FilterXScope *scope, LogMessage *msg);
const gchar *filterx_eval_get_last_error(void);
void filterx_eval_clear_errors(void);

void filterx_eval_init_context(FilterXEvalContext *context, FilterXEvalContext *previous_context);
void filterx_eval_deinit_context(FilterXEvalContext *context);

#endif
