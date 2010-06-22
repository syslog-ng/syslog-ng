#include "cfg-parser.h"
#include "plugin.h"
#include "dbparser.h"

extern CfgParser dbparser_parser;

static Plugin dbparser_plugins[] =
{
  {
    .type = LL_CONTEXT_PARSER,
    .name = "db-parser",
    .parser = &dbparser_parser,
  },
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  log_db_parser_global_init();
  plugin_register(cfg, dbparser_plugins, G_N_ELEMENTS(dbparser_plugins));
  return TRUE;
}
