typedef struct _TFCondState
{
  TFSimpleFuncState super;
  FilterExprNode *filter;
} TFCondState;

gboolean
tf_cond_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFCondState *state = (TFCondState *) s;
  CfgLexer *lexer;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  lexer = cfg_lexer_new_buffer(argv[0], strlen(argv[0]));
  if (!cfg_run_parser(parent->cfg, lexer, &filter_expr_parser, (gpointer *) &state->filter, NULL))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Error parsing conditional filter expression");
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, s, parent, argc - 1, &argv[1], error))
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
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  if (argc < 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(grep) requires at least two arguments");
      return FALSE;
    }
  return tf_cond_prepare(self, s, parent, argc, argv, error);
}

void
tf_grep_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  gint i, msg_ndx;
  gboolean first = TRUE;
  TFCondState *state = (TFCondState *) s;

  for (msg_ndx = 0; msg_ndx < args->num_messages; msg_ndx++)
    {
      LogMessage *msg = args->messages[msg_ndx];

      if (filter_expr_eval(state->filter, msg))
        {
          for (i = 0; i < state->super.argc; i++)
            {
              if (!first)
                g_string_append_c(result, ',');

              /* NOTE: not recursive, as the message context is just one message */
              log_template_append_format(state->super.argv[i], msg, args->opts, args->tz, args->seq_num, args->context_id, result);
              first = FALSE;
            }
        }
    }
}

TEMPLATE_FUNCTION(TFCondState, tf_grep, tf_grep_prepare, NULL, tf_grep_call, tf_cond_free_state, NULL);

gboolean
tf_if_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc != 3)
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
  LogMessage *msg;

  /* always apply to the last message */
  msg = args->messages[args->num_messages - 1];
  if (filter_expr_eval(state->filter, msg))
    {
      log_template_append_format(state->super.argv[0], msg, args->opts, args->tz, args->seq_num, args->context_id, result);
    }
  else
    {
      log_template_append_format(state->super.argv[1], msg, args->opts, args->tz, args->seq_num, args->context_id, result);
    }
}

TEMPLATE_FUNCTION(TFCondState, tf_if, tf_if_prepare, NULL, tf_if_call, tf_cond_free_state, NULL);
