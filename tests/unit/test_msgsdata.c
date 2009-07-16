#include "logmsg.h"
#include "apphook.h"

#include <stdlib.h>
#include <string.h>

void
testcase_update(const gchar *msg, const gchar *expected_sd_str, gchar *elem_name1, ...)
{
  LogMessage *logmsg;
  GString *sd_str = g_string_new("");
  va_list va;
  gchar *elem, *param, *value;

  va_start(va, elem_name1);

  logmsg = log_msg_new(msg, strlen(msg), NULL, LP_SYSLOG_PROTOCOL, NULL, -1, 0xFFFF);

  elem = elem_name1;
  param = va_arg(va, char *);
  value = va_arg(va, char *);
  while (elem)
    {
      log_msg_update_sdata(logmsg, elem, param, value);
      elem = va_arg(va, char *);
      param = va_arg(va, char *);
      value = va_arg(va, char *);
    }

  log_msg_format_sdata(logmsg, sd_str);

  if (strcmp(sd_str->str, expected_sd_str) != 0)
    {
      fprintf(stderr, "FAIL: sdata update failed, sd_str=%s, expected=%s\n", sd_str->str, expected_sd_str);
      exit(1);
    }

  g_string_free(sd_str, TRUE);
  log_msg_unref(logmsg);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  testcase_update("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...",
                  "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"][meta sequenceId=\"11\"][syslog-ng param=\"value\"]",
                  "meta", "sequenceId", "11",
                  "syslog-ng", "param", "value",
                  NULL, NULL, NULL);

  app_shutdown();
  return 0;
}
