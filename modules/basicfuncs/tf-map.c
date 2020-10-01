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

typedef struct _MapState
{
  TFSimpleFuncState super;
  LogTemplate *template;
} MapState;

static gboolean
tf_map_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
               GError **error)
{
  MapState *state = (MapState *)s;

  if (argc != 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "Wrong number of arguments. Example: $(map template list).\n");
      return FALSE;
    }

  state->template = log_template_new(configuration, "map");
  if (!log_template_compile(state->template, argv[1], error))
    {
      log_template_unref(state->template);
      state->template = NULL;
      return FALSE;
    }

  memmove(&argv[1], &argv[2], sizeof(argv[0]) * (argc - 2));
  if (!tf_simple_func_prepare(self, s, parent, argc - 1, argv, error))
    return FALSE;

  return TRUE;
}

static void
tf_map_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  ListScanner scanner;
  gsize initial_len = result->len;
  MapState *state = (MapState *)s;
  GString *lst = args->argv[0];
  LogMessage *msg = args->messages[0];

  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, lst->str, lst->len);

  ScratchBuffersMarker mark;
  scratch_buffers_mark(&mark);

  while (list_scanner_scan_next(&scanner))
    {
      const gchar *current_value = list_scanner_get_current_value(&scanner);
      _append_comma_between_list_elements_if_needed(result, initial_len);
      GString *buffer = scratch_buffers_alloc();
      LogTemplateEvalOptions options = *args->options;
      options.context_id = current_value;
      log_template_format(state->template, msg, &options, buffer);
      str_repr_encode_append(result, buffer->str, -1, ",");
    }
  list_scanner_deinit(&scanner);

  scratch_buffers_reclaim_marked(mark);
}

static void
tf_map_free_state(gpointer s)
{
  MapState *state = (MapState *)s;

  log_template_unref(state->template);
  state->template = NULL;
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(MapState, tf_map, tf_map_prepare, tf_simple_func_eval,
                  tf_map_call, tf_map_free_state, NULL);
