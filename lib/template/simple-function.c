/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#include "template/simple-function.h"
#include "template/templates.h"
#include "scratch-buffers.h"

void
log_template_append_format_recursive(LogTemplate *self, const LogTemplateInvokeArgs *args, GString *result)
{
  log_template_append_format_with_context(self,
                                          args->messages, args->num_messages,
                                          args->opts, args->tz, args->seq_num, args->context_id, result);
}


/* simple template functions which take templates as arguments */

gboolean
tf_simple_func_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                       GError **error)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  state->argv_templates = g_malloc(sizeof(LogTemplate *) * (argc - 1));

  /* NOTE: the argv argument contains the function name as argv[0],
   * but the LogTemplate array doesn't. Thus the index is shifted by
   * one. */
  for (i = 0; i < argc - 1; i++)
    {
      state->argv_templates[i] = log_template_new(parent->cfg, NULL);
      log_template_set_escape(state->argv_templates[i], parent->escape);
      if (!log_template_compile(state->argv_templates[i], argv[i + 1], error))
        {
          state->argc = i + 1;
          goto error;
        }
    }
  state->argc = argc - 1;
  return TRUE;
error:
  return FALSE;
}

void
tf_simple_func_eval(LogTemplateFunction *self, gpointer s, LogTemplateInvokeArgs *args)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  g_assert(state->argc <= TEMPLATE_INVOKE_MAX_ARGS);
  for (i = 0; i < state->argc; i++)
    {
      args->argv[i] = scratch_buffers_alloc();
      log_template_append_format_recursive(state->argv_templates[i], args, args->argv[i]);
    }
}

void
tf_simple_func_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFSimpleFunc simple_func = (TFSimpleFunc) self->arg;
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;

  simple_func(args->messages[args->num_messages-1], state->argc, (GString **) args->argv, result);
}

void
tf_simple_func_free_state(gpointer s)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  for (i = 0; i < state->argc; i++)
    {
      if (state->argv_templates[i])
        log_template_unref(state->argv_templates[i]);
    }
  g_free(state->argv_templates);
}
