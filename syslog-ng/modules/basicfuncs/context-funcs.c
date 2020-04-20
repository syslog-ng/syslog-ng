/*
 * Copyright (c) 2012-2016 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

static gboolean
tf_context_length_prepare(LogTemplateFunction *self, gpointer s,
                          LogTemplate *parent, gint argc, gchar *argv[],
                          GError **error)
{
  return TRUE;
}

static void
tf_context_length_call(LogTemplateFunction *self, gpointer s,
                       const LogTemplateInvokeArgs *args, GString *result)
{
  g_string_append_printf(result, "%d", args->num_messages);
}

static void
tf_context_length_free_state(gpointer s)
{
}

TEMPLATE_FUNCTION(NULL, tf_context_length,
                  tf_context_length_prepare, NULL, tf_context_length_call,
                  tf_context_length_free_state, NULL);

/*
 * $(context-lookup [opts] filter $nv1 $n2 ...)
 *
 * Options:
 *  --max-count or -m          The maximum number of matches, 0 for unlimited
 *
 * Returns in a syslog-ng style list of all elements
 */
void
tf_context_lookup_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  gboolean first = TRUE;
  TFCondState *state = (TFCondState *) s;
  gint count = 0;
  GString *buf = g_string_sized_new(64);

  for (gint msg_ndx = 0; msg_ndx < args->num_messages; msg_ndx++)
    {
      LogMessage *msg = args->messages[msg_ndx];

      if (filter_expr_eval(state->filter, msg))
        {
          count++;
          for (gint i = 0; i < state->super.argc; i++)
            {
              if (!first)
                g_string_append_c(result, ',');

              /* NOTE: not recursive, as the message context is just one message */
              log_template_format(state->super.argv_templates[i], msg, args->opts, args->tz, args->seq_num, args->context_id, buf);
              str_repr_encode_append(result, buf->str, buf->len, ",");

              first = FALSE;
            }
          if (state->grep_max_count && count >= state->grep_max_count)
            break;
        }
    }
  g_string_free(buf, TRUE);
}

TEMPLATE_FUNCTION(TFCondState, tf_context_lookup, tf_grep_prepare, NULL, tf_context_lookup_call, tf_cond_free_state,
                  NULL);

/*
 * $(context-values $nv1 $n2 ...)
 *
 * Returns in a syslog-ng style list of all specified name value pairs in the entire context.
 */
void
tf_context_values_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  gboolean first = TRUE;
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  GString *buf = g_string_sized_new(64);

  for (gint msg_ndx = 0; msg_ndx < args->num_messages; msg_ndx++)
    {
      LogMessage *msg = args->messages[msg_ndx];

      for (gint i = 0; i < state->argc; i++)
        {
          if (!first)
            g_string_append_c(result, ',');

          /* NOTE: not recursive, as the message context is just one message */
          log_template_format(state->argv_templates[i], msg, args->opts, args->tz, args->seq_num, args->context_id, buf);
          str_repr_encode_append(result, buf->str, buf->len, ",");

          first = FALSE;
        }
    }
  g_string_free(buf, TRUE);
}

TEMPLATE_FUNCTION(TFSimpleFuncState, tf_context_values, tf_simple_func_prepare, tf_simple_func_eval,
                  tf_context_values_call, tf_simple_func_free_state, NULL);
