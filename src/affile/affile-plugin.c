#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser affile_parser;

static Plugin affile_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "file",
    .parser = &affile_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "pipe",
    .parser = &affile_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "file",
    .parser = &affile_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "pipe",
    .parser = &affile_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, affile_plugins, G_N_ELEMENTS(affile_plugins));
  return TRUE;
}
