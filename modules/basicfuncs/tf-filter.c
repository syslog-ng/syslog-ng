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

static gboolean
tf_filter_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                  GError **error)
{
  if (argc != 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "Wrong number of arguments. Example: $(filter expr list).\n");
      return FALSE;
    }

  return tf_cond_prepare(self, s, parent, argc, argv, error);
}

static void
tf_filter_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  ListScanner scanner;
  gsize initial_len = result->len;
  TFCondState *state = (TFCondState *)s;
  GString *lst = args->argv[0];

  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, lst->str, lst->len);

  LogTemplateEvalOptions copy_options = *args->options;
  while (list_scanner_scan_next(&scanner))
    {
      const gchar *current = list_scanner_get_current_value(&scanner);
      copy_options.context_id = current;
      gboolean filter_result = filter_expr_eval_with_context(state->filter, args->messages, args->num_messages,
                                                             &copy_options);
      if (filter_result)
        {
          _append_comma_between_list_elements_if_needed(result, initial_len);
          g_string_append(result, current);
        }
    }
  list_scanner_deinit(&scanner);
}

TEMPLATE_FUNCTION(TFCondState, tf_filter, tf_filter_prepare, tf_simple_func_eval,
                  tf_filter_call, tf_cond_free_state, NULL);
