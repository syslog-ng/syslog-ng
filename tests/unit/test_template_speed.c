#include "syslog-ng.h"
#include "logmsg.h"
#include "templates.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "timeutils.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

gboolean success = TRUE;
gboolean verbose = FALSE;

/* Beginning of Message character encoded in UTF8 */
#define BOM "\xEF\xBB\xBF"

#define BENCHMARK_COUNT 1000

void
testcase(const gchar *msg_str, guint32 parse_flags, gchar *template)
{
  LogTemplate *templ;
  LogMessage *msg;
  GString *res = g_string_sized_new(128);
  static TimeZoneInfo *tzinfo = NULL;
  gint i;
  GTimeVal start, end;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), parse_flags, NULL, -1, 0xFFFF);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE2"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE3"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE4"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE5"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE6"), "value", -1);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE7"), "value", -1);
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].time.tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].time.tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(1139684315);

  templ = log_template_new("dummy", template);
  g_get_current_time(&start);

  for (i = 0; i < BENCHMARK_COUNT; i++)
    {
      log_template_format(templ, msg, LT_ESCAPE, TS_FMT_ISO, tzinfo, 3, 0, res);
    }
  g_get_current_time(&end);
  printf("%-60.*s speed: %.3f msg/sec\n", (int) strlen(template) - 1, template, i * 1e6 / g_time_val_diff(&end, &start));

  log_template_unref(templ);
  g_string_free(res, TRUE);
  log_msg_unref(msg);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  GlobalConfig dummy;

  if (argc > 1)
    verbose = TRUE;

  configuration = &dummy;
  dummy.version = 0x0300;

  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "<$PRI>$DATE $HOST $MSGHDR$MSG\n");

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "$DATE $HOST $MSGHDR$MSG\n");

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "$DATE $HOST $MSGHDR$MSG value\n");

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "$DATE $HOST $MSGHDR$MSG ${APP.VALUE}\n");

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "${APP.VALUE} ${APP.VALUE2}\n");

  testcase("<155>2006-02-11T10:34:56.156+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép", 0,
           "$DATE $FACILITY.$PRIORITY $HOST $MSGHDR$MSG $SEQNO\n");

  testcase("<155>1 2006-02-11T10:34:56.156+01:00 bzorp syslog-ng 23323 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] " BOM "árvíztűrőtükörfúrógép", LP_SYSLOG_PROTOCOL,
           "<$PRI>$DATE $HOST $MSGHDR$MSG\n");

  testcase("<155>1 2006-02-11T10:34:56.156+01:00 bzorp syslog-ng 23323 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] " BOM "árvíztűrőtükörfúrógép", LP_SYSLOG_PROTOCOL,
           "$DATE $HOST $PROGRAM $PID $MSGID $SDATA $MSG\n");

  testcase("<155>1 2006-02-11T10:34:56.156+01:00 bzorp syslog-ng 23323 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] " BOM "árvíztűrőtükörfúrógép", LP_SYSLOG_PROTOCOL,
           "$DATE $HOST $PROGRAM $PID $MSGID $MSG\n");

  testcase("<155>1 2006-02-11T10:34:56.156+01:00 bzorp syslog-ng 23323 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] " BOM "árvíztűrőtükörfúrógép", LP_SYSLOG_PROTOCOL,
           "$DATE ${HOST:--} ${PROGRAM:--} ${PID:--} ${MSGID:--} ${SDATA:--} $MSG\n");

  app_shutdown();

  if (success)
    return 0;
  return 1;
}
