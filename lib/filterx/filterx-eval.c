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
#include "filterx/filterx-eval.h"
#include "filterx/filterx-expr.h"
#include "scratch-buffers.h"
#include "tls-support.h"

TLS_BLOCK_START
{
  FilterXEvalContext *eval_context;
}
TLS_BLOCK_END;

#define eval_context __tls_deref(eval_context)

FilterXEvalContext *
filterx_eval_get_context(void)
{
  return eval_context;
}

FilterXScope *
filterx_eval_get_scope(void)
{
  if (eval_context)
    return eval_context->scope;
  return NULL;
}

void
filterx_eval_set_context(FilterXEvalContext *context)
{
  eval_context = context;
}

static gboolean
_evaluate_statement(FilterXExpr *expr)
{
  FilterXObject *res = filterx_expr_eval(expr);
  gboolean success = FALSE;

  if (res)
    success = filterx_object_truthy(res);

  if (!success || trace_flag)
    {
      GString *buf = scratch_buffers_alloc();

      if (res && !filterx_object_repr(res, buf))
        {
          LogMessageValueType t;
          if (!filterx_object_marshal(res, buf, &t))
            g_assert_not_reached();
        }

      if (!success)
        msg_debug("Filterx expression failed",
                  evt_tag_printf("expr", "%s:%d:%d| %s",
                                 expr->lloc.name, expr->lloc.first_line, expr->lloc.first_column,
                                 expr->expr_text ? : "n/a"),
                  evt_tag_str("status", res == NULL ? "error" : "falsy"),
                  evt_tag_mem("value", buf->str, buf->len));
      else
        msg_trace("FILTERX",
                  evt_tag_printf("expr", "%s:%d:%d| %s",
                                 expr->lloc.name, expr->lloc.first_line, expr->lloc.first_column,
                                 expr->expr_text ? : "n/a"),
                  evt_tag_str("status", res == NULL ? "error" : (success ? "truthy" : "falsy")),
                  evt_tag_mem("value", buf->str, buf->len),
                  evt_tag_printf("result", "%p", res));
    }

  filterx_object_unref(res);
  return success;
}


gboolean
filterx_eval_exec_statements(FilterXScope *scope, GList *statements, LogMessage *msg)
{
  FilterXEvalContext local_context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };
  filterx_eval_set_context(&local_context);
  gboolean success = FALSE;
  for (GList *l = statements; l; l = l->next)
    {
      FilterXExpr *expr = l->data;
      if (!_evaluate_statement(expr))
        {
          goto fail;
        }
    }
  /* NOTE: we only store the results into the message if the entire evaluation was successful */
  success = TRUE;
fail:
  filterx_eval_set_context(NULL);
  return success;
}

void
filterx_eval_sync_scope_and_message(FilterXScope *scope, LogMessage *msg)
{
  filterx_scope_sync_to_message(scope, msg);
}
