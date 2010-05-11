#include "syslog-ng.h"
#include "logwriter.h"
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
LogParseOptions parse_options;

LogMessage *
init_msg(gchar *msg_string, gboolean use_syslog_protocol)
{
  LogMessage *msg;

  if (use_syslog_protocol)
    parse_options.flags |= LP_SYSLOG_PROTOCOL;
  else
    parse_options.flags &= ~LP_SYSLOG_PROTOCOL;
  msg = log_msg_new(msg_string, strlen(msg_string), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE"), "value", 5);
  log_msg_set_match(msg, 0, "whole-match", 11);
  log_msg_set_match(msg, 1, "first-match", 11);

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", 9);
  msg->timestamps[LM_TS_RECVD].time.tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].time.tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(1139684315);
  return msg;
}

void
testcase(gchar *msg_string, gchar *template, gboolean use_syslog_protocol,gchar *expected_value)
{
  LogTemplate *templ = NULL;
  LogWriter *writer = NULL;
  GString *res = g_string_sized_new(128);
  GError *error = NULL;
  LogMessage *msg = NULL;
  LogWriterOptions opt = {0};
  guint writer_flags = 0;

  static TimeZoneInfo *tzinfo = NULL;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);
  opt.options = LWO_NO_MULTI_LINE | LWO_NO_STATS | LWO_SHARE_STATS;
  opt.send_time_zone_info=tzinfo;

  if (use_syslog_protocol)
    {
      opt.options |= LWO_SYSLOG_PROTOCOL;
      writer_flags = LW_SYSLOG_PROTOCOL;
    }
  if (template)
    {
      templ = log_template_new("dummy", template);
      log_template_compile(templ,&error);
    }
  opt.template = templ;
  msg = init_msg(msg_string, use_syslog_protocol);
  writer = (LogWriter*)log_writer_new(writer_flags);
  if (writer)
    {
      writer->options = &opt;
      log_writer_format_log(writer,msg,res);
      if(strcmp(res->str,expected_value)!=0)
        {
          fprintf(stderr,"Testcase failed; result: %s, expected: %s\n",res->str,expected_value);
          exit(1);
        }
    }
  else
    {
      fprintf(stderr,"Error: Can't create log_writer\n");
      exit(1);
    }
  if (templ)
    log_template_unref(templ);
  if (writer)
    free(writer);
  if (msg)
    free(msg);
  g_string_free(res, TRUE);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  char *msg_syslog_str = "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...";
  char *expeted_msg_syslog_str = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...\n";
  char *expeted_msg_syslog_str_t = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n";

  char *msg_syslog_empty_str ="<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]";
  char *expected_msg_syslog_empty_str ="<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \n";
  char *expected_msg_syslog_empty_str_t ="<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47\n";

  char *msg_bsd_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  char *expected_msg_bsd_str = " bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";
  char *expected_msg_bsd_str_t = "23323 árvíztűrőtükörfúrógép";

  char *msg_bsd_empty_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:";
  char *expected_msg_bsd_empty_str = " bzorp syslog-ng[23323]:\n";
  char *expected_msg_bsd_empty_str_t = "23323";
  GlobalConfig dummy;

  if (argc > 1)
    verbose = TRUE;

  configuration = &dummy;
  dummy.version = 0x0300;
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  log_parse_syslog_options_defaults(&parse_options);
  testcase(msg_syslog_str,NULL,TRUE,expeted_msg_syslog_str);
  testcase(msg_syslog_str,"$MSGID $MSG",TRUE,expeted_msg_syslog_str_t);
  testcase(msg_syslog_empty_str,NULL,TRUE,expected_msg_syslog_empty_str);
  testcase(msg_syslog_empty_str,"$MSGID",TRUE,expected_msg_syslog_empty_str_t);

  testcase(msg_bsd_str,NULL,FALSE,expected_msg_bsd_str);
  testcase(msg_bsd_str,"$PID $MSG",FALSE,expected_msg_bsd_str_t);
  testcase(msg_bsd_empty_str,NULL,FALSE,expected_msg_bsd_empty_str);
  testcase(msg_bsd_empty_str,"$PID",FALSE,expected_msg_bsd_empty_str_t);

  app_shutdown();
  return 0;
}
