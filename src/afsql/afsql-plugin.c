#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser afsql_parser;

static Plugin afsql_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "sql",
    .parser = &afsql_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, afsql_plugins, G_N_ELEMENTS(afsql_plugins));
  return TRUE;
}
