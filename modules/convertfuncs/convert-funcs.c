#include "plugin.h"
#include "cfg.h"
#include "gsocket.h"
#include "value-pairs.h"

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

static Plugin convert_func_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_ipv4_to_int, "ipv4-to-int"),
  TEMPLATE_FUNCTION_PLUGIN(tf_format_welf, "format-welf"),
};

gboolean
convertfuncs_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, convert_func_plugins, G_N_ELEMENTS(convert_func_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "convertfuncs",
  .version = VERSION,
  .description = "The convertfuncs module provides template functions that perform some kind of data conversion from one representation to the other.",
  .core_revision = SOURCE_REVISION,
  .plugins = convert_func_plugins,
  .plugins_len = G_N_ELEMENTS(convert_func_plugins),
};
