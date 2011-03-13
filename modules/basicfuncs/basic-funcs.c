#include "plugin.h"
#include "templates.h"
#include "filter.h"
#include "filter-expr-parser.h"
#include "cfg.h"

static void
tf_echo(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append_len(result, argv[i]->str, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_echo);

typedef struct _TFCondState
{
  FilterExprNode *filter;
  gint argc;
  LogTemplate *argv[0];
} TFCondState;

void
tf_cond_free_state(TFCondState *args)
{
  gint i;

  if (args->filter)
    filter_expr_unref(args->filter);
  for (i = 0; i < args->argc; i++)
    {
      if (args->argv[i])
        log_template_unref(args->argv[i]);
    }
  g_free(args);
}

gboolean
tf_cond_prepare(LogTemplateFunction *self, LogTemplate *parent, gint argc, gchar *argv[], gpointer *state, GDestroyNotify *state_destroy, GError **error)
{
  TFCondState *args;
  CfgLexer *lexer;
  gint i;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  args = g_malloc0(sizeof(TFCondState) + (argc - 1) * sizeof(LogTemplate *));
  args->argc = argc - 1;
  lexer = cfg_lexer_new_buffer(argv[0], strlen(argv[0]));
  if (!cfg_run_parser(parent->cfg, lexer, &filter_expr_parser, (gpointer *) &args->filter))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Error parsing conditional filter expression");
      goto error;
    }
  for (i = 1; i < argc; i++)
    {
      args->argv[i - 1] = log_template_new(parent->cfg, NULL, argv[i]);
      log_template_set_escape(args->argv[i - 1], TRUE);
      if (!log_template_compile(args->argv[i - 1], error))
        goto error;
    }
  *state = args;
  *state_destroy = (GDestroyNotify) tf_cond_free_state;
  return TRUE;
 error:
  tf_cond_free_state(args);
  return FALSE;

}

void
tf_cond_eval(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs, LogMessage **messages, gint num_messages, LogTemplateOptions *opts, gint tz, gint seq_num, const gchar *context_id)
{
  return;
}

gboolean
tf_grep_prepare(LogTemplateFunction *self, LogTemplate *parent, gint argc, gchar *argv[], gpointer *state, GDestroyNotify *state_destroy, GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  if (argc < 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(grep) requires at least two arguments");
      return FALSE;
    }
  return tf_cond_prepare(self, parent, argc, argv, state, state_destroy, error);
}


void
tf_grep_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs, LogMessage **messages, gint num_messages, LogTemplateOptions *opts, gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  gint i, msg_ndx;
  gboolean first = TRUE;
  TFCondState *args = (TFCondState *) state;

  for (msg_ndx = 0; msg_ndx < num_messages; msg_ndx++)
    {
      LogMessage *msg = messages[msg_ndx];

      if (filter_expr_eval(args->filter, msg))
        {
          for (i = 0; i < args->argc; i++)
            {
              if (!first)
                g_string_append_c(result, ',');
              log_template_append_format(args->argv[i], msg, opts, tz, seq_num, context_id, result);
              first = FALSE;
            }
        }
    }
}

gboolean
tf_if_prepare(LogTemplateFunction *self, LogTemplate *parent, gint argc, gchar *argv[], gpointer *state, GDestroyNotify *state_destroy, GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc != 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(if) requires three arguments");
      return FALSE;
    }
  return tf_cond_prepare(self, parent, argc, argv, state, state_destroy, error);
}

TEMPLATE_FUNCTION(tf_grep, tf_grep_prepare, tf_cond_eval, tf_grep_call, NULL);

void
tf_if_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs, LogMessage **messages, gint num_messages, LogTemplateOptions *opts, gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  TFCondState *args = (TFCondState *) state;
  LogMessage *msg;

  /* always apply to the last message */
  msg = messages[num_messages - 1];
  if (filter_expr_eval(args->filter, msg))
    {
      log_template_append_format(args->argv[0], msg, opts, tz, seq_num, context_id, result);
    }
  else
    {
      log_template_append_format(args->argv[1], msg, opts, tz, seq_num, context_id, result);
    }
}

TEMPLATE_FUNCTION(tf_if, tf_if_prepare, tf_cond_eval, tf_if_call, NULL);


static Plugin basicfuncs_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_echo, "echo"),
  TEMPLATE_FUNCTION_PLUGIN(tf_grep, "grep"),
  TEMPLATE_FUNCTION_PLUGIN(tf_if, "if"),
};

gboolean
basicfuncs_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, basicfuncs_plugins, G_N_ELEMENTS(basicfuncs_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "basicfuncs",
  .version = VERSION,
  .description = "The basicfuncs module provides various template functions for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = basicfuncs_plugins,
  .plugins_len = G_N_ELEMENTS(basicfuncs_plugins),
};
