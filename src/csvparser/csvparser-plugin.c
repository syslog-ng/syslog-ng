#include "cfg-parser.h"
#include "plugin.h"
#include "csvparser.h"

extern CfgParser csvparser_parser;

static Plugin csvparser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "csv-parser",
    .parser = &csvparser_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, csvparser_plugins, G_N_ELEMENTS(csvparser_plugins));
  return TRUE;
}
