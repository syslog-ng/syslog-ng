#include "syslog-ng.h"
#include "logmsg.h"
#include "templates.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>


int 
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  LogTemplate *templ;
  GString *res = g_string_sized_new(128);
  char *msg_str = "<7>Feb  2 10:34:56 bzorp syslog-ng[23323]: test message";
  char *expected_format = "syslog-ng/var/log/messages/bzorp/0202valami";
  
  msg = log_msg_new(msg_str, strlen(msg_str), NULL, 0);
  templ = log_template_new("dummy", "$PROGRAM/var/log/messages/$HOST/$MONTH$DAY$QQQQQvalami");
  log_template_format(templ, msg, 0, TS_FMT_BSD, -1, res);
  if (strcmp(res->str, expected_format) != 0)
    {
      printf("template test failed: %s <=> %s\n", res->str, expected_format);
      return 1;
    }
  return 0;
}
