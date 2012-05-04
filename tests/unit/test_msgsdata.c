#include "logmsg.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "testutils.h"

#include <stdlib.h>
#include <string.h>

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

void
testcase_update_sdata(const gchar *msg, const gchar *expected_sd_str, gchar *elem_name1, ...)
{
  LogMessage *logmsg;
  GString *sd_str = g_string_new("");
  va_list va;
  gchar *elem, *param, *value;

  parse_options.flags |= LP_SYSLOG_PROTOCOL;

  va_start(va, elem_name1);

  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);

  elem = elem_name1;
  param = va_arg(va, char *);
  value = va_arg(va, char *);
  while (elem)
    {
      gchar sd_name[64];

      g_snprintf(sd_name, sizeof(sd_name), ".SDATA.%s.%s", elem, param);
      log_msg_set_value(logmsg, log_msg_get_value_handle(sd_name), value, -1);
      elem = va_arg(va, char *);
      param = va_arg(va, char *);
      value = va_arg(va, char *);
    }

  log_msg_format_sdata(logmsg, sd_str, 0);

  assert_string(sd_str->str, expected_sd_str, "sdata update failed");

  g_string_free(sd_str, TRUE);
  log_msg_unref(logmsg);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  init_and_load_syslogformat_module();

  testcase_update_sdata("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...",
                  "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"][meta sequenceId=\"11\"][syslog-ng param=\"value\"]",
                  "meta", "sequenceId", "11",
                  "syslog-ng", "param", "value",
                  NULL, NULL, NULL);

  deinit_syslogformat_module();
  app_shutdown();
  return 0;
}
