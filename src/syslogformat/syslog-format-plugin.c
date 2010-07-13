
#include "syslog-format.h"
#include "messages.h"
#include "plugin.h"

static MsgFormatHandler syslog_handler =
{
  .parse = &syslog_format_handler
};

static MsgFormatHandler *
syslog_format_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &syslog_handler;
}

static Plugin syslog_format_plugin =
{
  .type = LL_CONTEXT_FORMAT,
  .name = "syslog",
  .construct = &syslog_format_construct,
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &syslog_format_plugin, 1);
  return TRUE;
}
