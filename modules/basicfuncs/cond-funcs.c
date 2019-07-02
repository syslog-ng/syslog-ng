/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2013-2014 Gergely Nagy
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

typedef struct _TFCondState
{
  TFSimpleFuncState super;
  FilterExprNode *filter;
  gint grep_max_count;
} TFCondState;

gboolean
tf_cond_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFCondState *state = (TFCondState *) s;
  CfgLexer *lexer;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  lexer = cfg_lexer_new_buffer(parent->cfg, argv[1], strlen(argv[1]));
  if (!cfg_run_parser(parent->cfg, lexer, &filter_expr_parser, (gpointer *) &state->filter, NULL))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(%s) Error parsing conditional filter expression", argv[0]);
      return FALSE;
    }
  if (!filter_expr_init(state->filter, parent->cfg))
    return FALSE;
  memmove(&argv[1], &argv[2], sizeof(argv[0]) * (argc - 2));
  if (!tf_simple_func_prepare(self, s, parent, argc - 1, argv, error))
    return FALSE;

  return TRUE;

}

static void
tf_cond_free_state(gpointer s)
{
  TFCondState *state = (TFCondState *) s;

  if (state->filter)
    filter_expr_unref(state->filter);
  tf_simple_func_free_state(&state->super);
}

gboolean
tf_grep_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFCondState *state = (TFCondState *) s;
  GOptionContext *ctx;
  gint max_count = 0;
  GOptionEntry grep_options[] =
  {
    { "max-count", 'm', 0, G_OPTION_ARG_INT, &max_count, NULL, NULL },
    { NULL }
  };

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  ctx = g_option_context_new(argv[0]);
  g_option_context_add_main_entries(ctx, grep_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      g_free(argv);
      return FALSE;
    }
  g_option_context_free(ctx);

  if (argc < 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(%s) requires at least two arguments", argv[0]);
      return FALSE;
    }
  state->grep_max_count = max_count;
  return tf_cond_prepare(self, s, parent, argc, argv, error);
}

/*
 * $(grep [opts] filter $nv1 $n2 ...)
 *
 * Options:
 *  --max-count or -m          The maximum number of matches, 0 for unlimited
 */
void
tf_grep_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  gint i, msg_ndx;
  gboolean first = TRUE;
  TFCondState *state = (TFCondState *) s;
  gint count = 0;

  for (msg_ndx = 0; msg_ndx < args->num_messages; msg_ndx++)
    {
      LogMessage *msg = args->messages[msg_ndx];

      if (filter_expr_eval(state->filter, msg))
        {
          count++;
          for (i = 0; i < state->super.argc; i++)
            {
              if (!first)
                g_string_append_c(result, ',');

              /* NOTE: not recursive, as the message context is just one message */
              log_template_append_format(state->super.argv_templates[i], msg,
                                         args->opts, args->tz, args->seq_num, args->context_id,
                                         result);
              first = FALSE;
            }
          if (state->grep_max_count && count >= state->grep_max_count)
            break;
        }
    }
}

TEMPLATE_FUNCTION(TFCondState, tf_grep, tf_grep_prepare, NULL, tf_grep_call, tf_cond_free_state, NULL);

gboolean
tf_if_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc != 4)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(if) requires three arguments");
      return FALSE;
    }
  return tf_cond_prepare(self, s, parent, argc, argv, error);
}

void
tf_if_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFCondState *state = (TFCondState *) s;

  if (filter_expr_eval_with_context(state->filter, args->messages, args->num_messages))
    {
      log_template_append_format_with_context(state->super.argv_templates[0],
                                              args->messages, args->num_messages, args->opts, args->tz,
                                              args->seq_num, args->context_id,
                                              result);
    }
  else
    {
      log_template_append_format_with_context(state->super.argv_templates[1],
                                              args->messages, args->num_messages, args->opts, args->tz,
                                              args->seq_num, args->context_id,
                                              result);
    }
}

TEMPLATE_FUNCTION(TFCondState, tf_if, tf_if_prepare, NULL, tf_if_call, tf_cond_free_state, NULL);

static void
tf_or (LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      if (argv[i]->len == 0)
        continue;

      g_string_append_len (result, argv[i]->str, argv[i]->len);
      break;
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_or);
