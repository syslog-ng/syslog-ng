#include "cfg-parser.h"
#include "plugin.h"
#include "plugin-types.h"

extern CfgParser http_test_slots_parser;

static Plugin http_test_slots_plugins[] =
{
  {
    .type = LL_CONTEXT_INNER_DEST,
    .name = "http_test_slots",
    .parser = &http_test_slots_parser
  }
};

#ifndef STATIC
const ModuleInfo module_info =
{
  .canonical_name = "http-test-slots",
  .version = SYSLOG_NG_VERSION,
  .description = "poc http slots module",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = http_test_slots_plugins,
  .plugins_len = G_N_ELEMENTS(http_test_slots_plugins)
};
#endif

gboolean
http_test_slots_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, http_test_slots_plugins, G_N_ELEMENTS(http_test_slots_plugins));

  return TRUE;
}
