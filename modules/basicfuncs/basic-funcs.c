#include "plugin.h"
#include "templates.h"
#include "filter.h"
#include "filter-expr-parser.h"
#include "cfg.h"
#include "str-format.h"

#include <stdlib.h>
#include <errno.h>

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

static gboolean
tf_parse_int(const gchar *s, long *d)
{
  gchar *endptr;
  glong val;

  errno = 0;
  val = strtoll(s, &endptr, 10);

  if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
      || (errno != 0 && val == 0))
    return FALSE;

  if (endptr == s || *endptr != '\0')
    return FALSE;

  *d = val;
  return TRUE;
}

static gboolean
tf_num_parse(gint argc, GString *argv[],
	     const gchar *func_name, glong *n, glong *m)
{
  if (argc != 2)
    {
      msg_debug("Template function requires two arguments.",
		evt_tag_str("function", func_name), NULL);
      return FALSE;
    }

  if (!tf_parse_int(argv[0]->str, n))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[0]->str), NULL);
      return FALSE;
    }

  if (!tf_parse_int(argv[1]->str, m))
    {
      msg_debug("Parsing failed, template function's first argument is not a number",
		evt_tag_str("function", func_name),
		evt_tag_str("arg1", argv[1]->str), NULL);
      return FALSE;
    }

  return TRUE;
}

static void
tf_num_plus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "+", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n + m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_plus);

static void
tf_num_minus(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "-", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n - m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_minus);

static void
tf_num_multi(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "*", &n, &m))
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n * m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_multi);

static void
tf_num_div(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "/", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_int32_padded(result, 0, ' ', 10, n / m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_div);

static void
tf_num_mod(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong n, m;

  if (!tf_num_parse(argc, argv, "%", &n, &m) || !m)
    {
      g_string_append_len(result, "NaN", 3);
      return;
    }

  format_uint32_padded(result, 0, ' ', 10, n % m);
}

TEMPLATE_FUNCTION_SIMPLE(tf_num_mod);

static void
tf_substr(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  glong start, len;

  /*
   * We need to cast argv[0]->len, which is gsize (so unsigned) type, to a
   * signed type, so we can compare negative offsets/lengths with string
   * length. But if argv[0]->len is bigger than G_MAXLONG, this can lead to
   * completely wrong calculations, so we'll just return nothing (alternative
   * would be to return original string and perhaps print an error...)
   */
  if (argv[0]->len >= G_MAXLONG) {
    msg_error("$(substr) error: string is too long", NULL);
    return;
  }

  /* check number of arguments */
  if (argc < 2 || argc > 3)
    return;

  /* get offset position from second argument */
  if (!tf_parse_int(argv[1]->str, &start)) {
    msg_error("$(substr) parsing failed, start could not be parsed", evt_tag_str("start", argv[1]->str), NULL);
    return;
  }

  /* if we were called with >2 arguments, third was desired length */
  if (argc > 2) {
    if (!tf_parse_int(argv[2]->str, &len)) {
      msg_error("$(substr) parsing failed, length could not be parsed", evt_tag_str("length", argv[2]->str), NULL);
      return;
    }
  } else
    len = (glong)argv[0]->len;

  /*
   * if requested substring length is negative and beyond string start, do nothing;
   * also if it is greater than string length, limit it to string length
   */
  if (len < 0 && -(len) > (glong)argv[0]->len)
    return;
  else if (len > (glong)argv[0]->len)
    len = (glong)argv[0]->len;

  /*
   * if requested offset is beyond string length, do nothing;
   * also, if offset is negative and "before" string beginning, do nothing
   * (or should we perhaps align start with the beginning instead?)
   */
  if (start >= (glong)argv[0]->len)
    return;
  else if (start < 0 && -(start) > (glong)argv[0]->len)
    return;

  /* with negative length, see if we don't end up with start > end */
  if (len < 0 && ((start < 0 && start > len) ||
		  (start >= 0 && (len + ((glong)argv[0]->len) - start) < 0)))
    return;

  /* if requested offset is negative, move start it accordingly */
  if (start < 0) {
    start = start + (glong)argv[0]->len;
    /*
     * this shouldn't actually happen, as earlier we tested for
     * (start < 0 && -start > argv0len), but better safe than sorry
     */
    if (start < 0)
      start = 0;
  }

  /*
   * if requested length is negative, "resize" len to include exactly as many
   * characters as needed from the end of the string, given our start position.
   * (start is always non-negative here already)
   */
  if (len < 0) {
    len = ((glong)argv[0]->len) - start + len;
    /* this also shouldn't happen, but - again - better safe than sorry */
    if (len < 0)
      return;
  }

  /* if we're beyond string end, do nothing */
  if (start >= (glong)argv[0]->len)
    return;

  /* if we want something beyond string end, do it only till the end */
  if (start + len > (glong)argv[0]->len)
    len = ((glong)argv[0]->len) - start;

  /* skip g_string_append_len if we ended up having to extract 0 chars */
  if (len == 0)
    return;

  g_string_append_len(result, argv[0]->str + start, len);
}

TEMPLATE_FUNCTION_SIMPLE(tf_substr);

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


static Plugin basicfuncs_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_echo, "echo"),
  TEMPLATE_FUNCTION_PLUGIN(tf_grep, "grep"),
  TEMPLATE_FUNCTION_PLUGIN(tf_if, "if"),
  TEMPLATE_FUNCTION_PLUGIN(tf_substr, "substr"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_plus, "+"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_minus, "-"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_multi, "*"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_div, "/"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_mod, "%"),
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
