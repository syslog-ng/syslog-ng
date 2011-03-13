#include "plugin.h"
#include "cfg.h"
#include "gsocket.h"

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

static Plugin convert_func_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_ipv4_to_int, "ipv4-to-int"),
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
