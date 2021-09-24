/*
 * Copyright (c) 2020 Balabit
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

typedef struct _IterateState
{
  TFSimpleFuncState super;
  GMutex mutex;
  GString *current;
  LogTemplate *template;
} IterateState;

static gboolean
tf_iterate_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                   GError **error)
{
  IterateState *state = (IterateState *)s;

  GOptionContext *ctx = g_option_context_new("iterate");

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      return FALSE;
    }

  if (argc != 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "Wrong number of arguments. Example: $(iterate template initial-value).\n");
      g_option_context_free(ctx);
      return FALSE;
    }

  state->template = log_template_new(configuration, "iterate");
  if (!log_template_compile(state->template, argv[1], error))
    {
      log_template_unref(state->template);
      state->template = NULL;
      g_option_context_free(ctx);
      return FALSE;
    }

  state->current = g_string_new(argv[2]);
  g_option_context_free(ctx);

  g_mutex_init(&state->mutex);

  return TRUE;
}

static void
update_current(LogTemplateFunction *self, IterateState *state, LogMessage *msg)
{
  gchar *current_value = g_strdup(state->current->str);

  g_string_assign(state->current, "");
  LogTemplateEvalOptions options = {NULL, LTZ_LOCAL, 0, current_value};
  log_template_format(state->template, msg, &options, state->current);

  g_free(current_value);
}

static void
tf_iterate_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  IterateState *state = (IterateState *)s;

  g_mutex_lock(&state->mutex);
  g_string_append(result, state->current->str);
  update_current(self, state, args->messages[0]);
  g_mutex_unlock(&state->mutex);
}

static void
tf_iterate_free_state(gpointer s)
{
  IterateState *state = (IterateState *)s;

  log_template_unref(state->template);
  state->template = NULL;
  g_string_free(state->current, TRUE);
  state->current = NULL;

  tf_simple_func_free_state(&state->super);
  g_mutex_clear(&state->mutex);
}

TEMPLATE_FUNCTION(IterateState, tf_iterate, tf_iterate_prepare, NULL,
                  tf_iterate_call, tf_iterate_free_state, NULL);
