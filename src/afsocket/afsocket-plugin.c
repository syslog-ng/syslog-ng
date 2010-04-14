#include "cfg-parser.h"
#include "plugin.h"

extern CfgParser afsocket_parser;

static Plugin afsocket_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "unix-stream",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "unix-dgram",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "tcp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "tcp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "udp",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
    .name = "udp6",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "syslog",
    .parser = &afsocket_parser,
  },
  {
    .type = LL_CONTEXT_DESTINATION,
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
