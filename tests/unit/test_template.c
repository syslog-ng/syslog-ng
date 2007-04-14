#include "syslog-ng.h"
#include "logmsg.h"
#include "templates.h"
#include "misc.h"
#include "macros.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

gboolean success = TRUE;

void
testcase(LogMessage *msg, gchar *template, gchar *expected)
{
  LogTemplate *templ;
  GString *res = g_string_sized_new(128);
  
  templ = log_template_new("dummy", template);
  log_template_format(templ, msg, MF_ESCAPE_RESULT, TS_FMT_BSD, -1, 3, res);
  
  if (strcmp(res->str, expected) != 0)
    {
      fprintf(stderr, "template test failed, template=%s, %s <=> %s\n", template, res->str, expected);
      success = FALSE;
    }
  else
    {
      fprintf(stderr, "template test success, template=%s => %s\n", template, expected);
    }
  log_template_unref(templ);
  g_string_free(res, TRUE);
}

int 
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép";
  
  putenv("TZ=CET");
  tzset();

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), 0, NULL);
  
  /* fix some externally or automatically defined values */
  g_string_assign(&msg->host_from, "kismacska");
  msg->recvd.time.tv_sec = 1139684315;
  msg->recvd.time.tv_usec = 639000;
  msg->recvd.zone_offset = get_local_timezone_ofs(1139684315);

  /* pri 3, fac 19 == local3 */

  testcase(msg, "$FACILITY", "local3");
  testcase(msg, "$FACILITY_NUM", "19");
  testcase(msg, "$PRIORITY", "err");
  testcase(msg, "$LEVEL", "err");
  testcase(msg, "$LEVEL_NUM", "3");
  testcase(msg, "$TAG", "9b");
  testcase(msg, "$PRI", "155");
  testcase(msg, "$DATE", "Feb 11 10:34:56");
  testcase(msg, "$FULLDATE", "2006 Feb 11 10:34:56");
  testcase(msg, "$ISODATE", "2006-02-11T10:34:56.000+01:00");
  testcase(msg, "$STAMP", "Feb 11 10:34:56");
  testcase(msg, "$YEAR", "2006");
  testcase(msg, "$MONTH", "02");
  testcase(msg, "$DAY", "11");
  testcase(msg, "$HOUR", "10");
  testcase(msg, "$MIN", "34");
  testcase(msg, "$SEC", "56");
  testcase(msg, "$WEEKDAY", "Sat");
  testcase(msg, "$WEEK", "06");
  testcase(msg, "$UNIXTIME", "1139650496");
  testcase(msg, "$TZOFFSET", "+01:00");
  testcase(msg, "$TZ", "+01:00");
  testcase(msg, "$R_DATE", "Feb 11 19:58:35");
  testcase(msg, "$R_FULLDATE", "2006 Feb 11 19:58:35");
  testcase(msg, "$R_ISODATE", "2006-02-11T19:58:35.639+01:00");
  testcase(msg, "$R_STAMP", "Feb 11 19:58:35");
  testcase(msg, "$R_YEAR", "2006");
  testcase(msg, "$R_MONTH", "02");
  testcase(msg, "$R_DAY", "11");
  testcase(msg, "$R_HOUR", "19");
  testcase(msg, "$R_MIN", "58");
  testcase(msg, "$R_SEC", "35");
  testcase(msg, "$R_WEEKDAY", "Sat");
  testcase(msg, "$R_WEEK", "06");
  testcase(msg, "$R_UNIXTIME", "1139684315");
  testcase(msg, "$R_TZOFFSET", "+01:00");
  testcase(msg, "$R_TZ", "+01:00");
  testcase(msg, "$S_DATE", "Feb 11 10:34:56");
  testcase(msg, "$S_FULLDATE", "2006 Feb 11 10:34:56");
  testcase(msg, "$S_ISODATE", "2006-02-11T10:34:56.000+01:00");
  testcase(msg, "$S_STAMP", "Feb 11 10:34:56");
  testcase(msg, "$S_YEAR", "2006");
  testcase(msg, "$S_MONTH", "02");
  testcase(msg, "$S_DAY", "11");
  testcase(msg, "$S_HOUR", "10");
  testcase(msg, "$S_MIN", "34");
  testcase(msg, "$S_SEC", "56");
  testcase(msg, "$S_WEEKDAY", "Sat");
  testcase(msg, "$S_WEEK", "06");
  testcase(msg, "$S_UNIXTIME", "1139650496");
  testcase(msg, "$S_TZOFFSET", "+01:00");
  testcase(msg, "$S_TZ", "+01:00");
  testcase(msg, "$HOST_FROM", "kismacska");
  testcase(msg, "$FULLHOST_FROM", "kismacska");
  testcase(msg, "$HOST", "bzorp");
  testcase(msg, "$FULLHOST", "bzorp");
  testcase(msg, "$PROGRAM", "syslog-ng");
  testcase(msg, "$PID", "23323");
  testcase(msg, "$MSG", "syslog-ng[23323]: árvíztűrőtükörfúrógép");
  testcase(msg, "$MSGONLY", "árvíztűrőtükörfúrógép");
  testcase(msg, "$MESSAGE", "syslog-ng[23323]: árvíztűrőtükörfúrógép");
  testcase(msg, "$SOURCEIP", "10.10.10.10");
  testcase(msg, "$PROGRAM/var/log/messages/$HOST/$HOST_FROM/$MONTH$DAY$QQQQQvalami", "syslog-ng/var/log/messages/bzorp/kismacska/0211valami");
  if (success)
    return 0;
  return 1;
}
