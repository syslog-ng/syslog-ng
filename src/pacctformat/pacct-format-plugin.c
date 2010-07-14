
#include "pacct-format.h"
#include "messages.h"
#include "plugin.h"

static MsgFormatHandler *
pacct_format_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &pacct_handler;
}

static Plugin pacct_format_plugin =
{
  .type = LL_CONTEXT_FORMAT,
  .name = "pacct",
  .construct = &pacct_format_construct,
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &pacct_format_plugin, 1);
  return TRUE;
}
