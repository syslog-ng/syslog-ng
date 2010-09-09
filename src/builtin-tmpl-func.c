#include "plugin.h"
#include "templates.h"

static void
tf_echo(GString *result, LogMessage *msg, gint argc, GString *argv[])
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append_len(result, argv[i]->str, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, ' ');
    }
}

TEMPLATE_FUNCTION(tf_echo);

static Plugin builtin_tmpl_func_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_echo, "echo"),
};

gboolean
builtin_tmpl_func_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, builtin_tmpl_func_plugins, G_N_ELEMENTS(builtin_tmpl_func_plugins));
  return TRUE;
}
