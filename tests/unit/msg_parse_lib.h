#include "cfg.h"
#include "plugin.h"
#include "logmsg.h"

#ifndef MSG_PARSE_LIB_H_INCLUDED
#define MSG_PARSE_LIB_H_INCLUDED

MsgFormatOptions parse_options;

void
init_and_load_syslogformat_module()
{
  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

void
deinit_syslogformat_module()
{
  if (configuration)
    cfg_free(configuration);
  configuration = NULL;
}

#endif
