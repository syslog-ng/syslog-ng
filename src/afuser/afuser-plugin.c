#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser afuser_parser;

static Plugin afuser_plugins[] =
{
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "usertty",
    .parser = &afuser_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, afuser_plugins, G_N_ELEMENTS(afuser_plugins));
  return TRUE;
}
