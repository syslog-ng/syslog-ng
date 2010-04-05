#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser afsocket_parser;

static Plugin afsocket_plugins[] =
{
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_SRC_DRIVER,
    .name = "syslog",
    .parser = &afsocket_parser,
  },
  {
    .type = PLUGIN_TYPE_DEST_DRIVER,
    .name = "syslog",
    .parser = &afsocket_parser,
  },
};

gboolean
syslogng_module_init(void)
{
  plugin_register(afsocket_plugins, G_N_ELEMENTS(afsocket_plugins));
  return TRUE;
}
