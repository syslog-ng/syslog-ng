#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser afprog_parser;

static Plugin afprog_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "program",
    .parser = &afprog_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "program",
    .parser = &afprog_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, afprog_plugins, G_N_ELEMENTS(afprog_plugins));
  return TRUE;
}
