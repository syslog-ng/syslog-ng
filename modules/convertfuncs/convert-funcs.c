#include "plugin.h"
#include "cfg.h"
#include "gsocket.h"
#include "value-pairs.h"
#include "str-format.h"

static void
tf_ipv4_to_int(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      struct in_addr ina;

      g_inet_aton(argv[i]->str, &ina);
      g_string_append_printf(result, "%lu", (gulong) ntohl(ina.s_addr));
      if (i < argc - 1)
        g_string_append_c(result, ',');
    }
}

TEMPLATE_FUNCTION_SIMPLE(tf_ipv4_to_int);

static gboolean
tf_format_welf_prepare(LogTemplateFunction *self, LogTemplate *parent,
                gint argc, gchar *argv[],
                gpointer *state, GDestroyNotify *state_destroy,
                GError **error)
{
  ValuePairs *vp;

  vp = value_pairs_new_from_cmdline (parent->cfg, argc, argv, error);
  if (!vp)
    return FALSE;

  *state = vp;
  *state_destroy = (GDestroyNotify) value_pairs_free;

  return TRUE;
}

static gboolean
tf_format_welf_foreach(const gchar *name, const gchar *value, gpointer user_data)
{

  GString *result=(GString *) user_data;
  gchar *escaped_value = g_strescape(value, NULL);

  if (result->len > 0)
    g_string_append(result," ");

  if (strchr(value, ' ') == NULL)
    g_string_append_printf(result, "%s=%s", name, escaped_value);
  else
    g_string_append_printf(result, "%s=\"%s\"", name, escaped_value);

  g_free(escaped_value);
  return FALSE;
}

static gint
tf_format_welf_strcmp(gconstpointer a, gconstpointer b)
{
  gchar *sa = (gchar *)a, *sb = (gchar *)b;

  if (strcmp (sa, "id") == 0)
    return -1;
  return strcmp(sa, sb);
}

static void
tf_format_welf_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
             LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
             gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  gint i;
  ValuePairs *vp = (ValuePairs *)state;

  for (i = 0; i < num_messages; i++)
    value_pairs_foreach_sorted(vp, tf_format_welf_strcmp,
                               tf_format_welf_foreach, messages[i], 0, result);
}

static void
tf_format_welf_eval (LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
              LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
              gint tz, gint seq_num, const gchar *context_id)
{
  return;
}

TEMPLATE_FUNCTION(tf_format_welf,tf_format_welf_prepare,tf_format_welf_eval,tf_format_welf_call,NULL);

typedef struct _SnareFormatOptions {
  GlobalConfig *cfg;
  LogTemplate *protocol_template;
  LogTemplate *event_template;
  LogTemplate *file_template;
  gchar         *delimiter;
  guint        criticality;
} SnareFormatOptions;

static gboolean
tf_format_snare_prepare(LogTemplateFunction *self, LogTemplate *parent,
                gint argc, gchar *argv[],
                gpointer *state, GDestroyNotify *state_destroy,
                GError **error)
{
  GError *err = NULL;
  gchar **cargv;
  gint cargc = argc + 1;
  GString *event_template_str = g_string_sized_new(512);
  SnareFormatOptions *options = g_new0(SnareFormatOptions,1);
  gint i;
  GOptionContext *ctx;
  GOptionGroup *og;

  GOptionEntry snare_options[] = {
    { "delimiter", 'd', 0, G_OPTION_ARG_STRING, &options->delimiter,
      NULL, NULL },
    { "criticality", 'c', 0, G_OPTION_ARG_INT, &options->criticality,
      NULL, NULL },
    { NULL }
  };

  options->delimiter = g_strdup("\\\t");
  options->criticality = 1;
  options->cfg = parent->cfg;

  cargv = g_new0 (gchar *, cargc + 1);
  for (i = 0; i < cargc; i++)
    cargv[i+1] = argv[i];
  cargv[0] = "snare-format";
  cargv[cargc] = NULL;

  ctx = g_option_context_new ("snare-format");
  og = g_option_group_new (NULL, NULL, NULL, NULL, NULL);
  g_option_group_add_entries (og, snare_options);
  g_option_context_set_main_group (ctx, og);

  if (!g_option_context_parse (ctx, &cargc, &cargv, error))
    {
      g_option_context_free (ctx);
      g_free(cargv);
      return FALSE;
    }
  g_option_context_free (ctx);

  g_string_append(event_template_str,"MSWinEventLog");
  g_string_append(event_template_str,options->delimiter);
  format_uint32_padded(event_template_str, 1, '0', 10, options->criticality);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_NAME %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_GLOBAL_COUNTER %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace \"$WEEKDAY $MONTHNAME $DAY $HOUR:$MIN:$SEC $YEAR\" %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_ID %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_PROVIDER %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_SNARE_USER %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_SID_TYPE %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_TYPE %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_HOST %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_CATEGORY %s \" \")",options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s%s$(replace $EVENT_MESSAGE %s \" \")",options->delimiter,options->delimiter,options->delimiter);
  g_string_append_printf(event_template_str,"%s$(replace $EVENT_CONTAINER_COUNTER %s \" \")",options->delimiter,options->delimiter);
  g_string_append(event_template_str,"\n");

  options->protocol_template = log_template_new(options->cfg,"snare_protocol_template");
  log_template_compile(options->protocol_template,"<${PRI}>${BSDDATE} ${HOST} ",&err);

  options->event_template = log_template_new(options->cfg,"snare_event_template");
  log_template_compile(options->event_template,event_template_str->str, &err);

  options->file_template = log_template_new(options->cfg,"snare_file_template");
  log_template_compile(options->file_template,"${FILE_NAME}: ${FILE_MESSAGE}\n",&err);
  self->arg = options;
  g_string_free(event_template_str,TRUE);
  return err == NULL;
}

static void
tf_format_snare_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
             LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
             gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  gint i;
  gssize len;
  static NVHandle hEventId = 0;
  SnareFormatOptions *options = self->arg;

  if (!hEventId)
    hEventId = log_msg_get_value_handle("EVENT_ID");
  for(i = 0; i < num_messages; i++)
  {
    LogMessage *msg = messages[i];
    log_template_append_format(options->protocol_template, msg, opts, tz, seq_num, context_id, result);
    log_msg_get_value(msg,hEventId,&len);
    if (len == 0)
    {
      log_template_append_format(options->file_template, msg, opts, tz, seq_num, context_id, result);
    }
    else
    {
      log_template_append_format(options->event_template, msg, opts, tz, seq_num, context_id, result);
    }
  }
  return;
}

static void
tf_format_snare_eval (LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
              LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
              gint tz, gint seq_num, const gchar *context_id)
{
  return;
}

TEMPLATE_FUNCTION(tf_format_snare,tf_format_snare_prepare,tf_format_snare_eval,tf_format_snare_call,NULL);

static void
tf_replace(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  gchar *value = argv[0]->str;
  if ((argc != 3) || (argv[1]->len != 1) || (argv[2]->len != 1))
  {
    g_string_append(result,"replace: syntax error. Syntax is: replace subject search_character replace_character");
    return;
  }
  gchar from = argv[1]->str[0];
  gchar to = argv[2]->str[0];
  gchar *p = value;
  while(*p)
  {
    if(*p == from)
      g_string_append_c(result,to);
    else
      g_string_append_c(result,*p);
    p++;
  }
  return;
}

TEMPLATE_FUNCTION_SIMPLE(tf_replace);

static Plugin convert_func_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_ipv4_to_int, "ipv4-to-int"),
  TEMPLATE_FUNCTION_PLUGIN(tf_format_welf, "format-welf"),
  TEMPLATE_FUNCTION_PLUGIN(tf_format_snare, "format-snare"),
  TEMPLATE_FUNCTION_PLUGIN(tf_replace,"replace"),
};

gboolean
convertfuncs_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, convert_func_plugins, G_N_ELEMENTS(convert_func_plugins));
  return TRUE;
}

#ifndef STATIC
const ModuleInfo module_info =
{
  .canonical_name = "convertfuncs",
  .version = VERSION,
  .description = "The convertfuncs module provides template functions that perform some kind of data conversion from one representation to the other.",
  .core_revision = SOURCE_REVISION,
  .plugins = convert_func_plugins,
  .plugins_len = G_N_ELEMENTS(convert_func_plugins),
};
#endif
