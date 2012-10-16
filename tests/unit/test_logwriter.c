#include "syslog-ng.h"
#include "logwriter.h"
#include "logmsg.h"
#include "templates.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "timeutils.h"
#include "plugin.h"
#include "logqueue-fifo.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

gboolean success = TRUE;
gboolean verbose = FALSE;
MsgFormatOptions parse_options;

LogMessage *
init_msg(gchar *msg_string, gboolean use_syslog_protocol, gboolean local_message)
{
  LogMessage *msg;

  if (use_syslog_protocol)
    parse_options.flags |= LP_SYSLOG_PROTOCOL;
  else
    parse_options.flags &= ~LP_SYSLOG_PROTOCOL;

  if (local_message)
    parse_options.flags |= LP_LOCAL;
  else
    parse_options.flags &= ~LP_LOCAL;

  msg = log_msg_new(msg_string, strlen(msg_string), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE"), "value", 5);
  log_msg_set_match(msg, 0, "whole-match", 11);
  log_msg_set_match(msg, 1, "first-match", 11);

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", 9);
  msg->timestamps[LM_TS_RECVD].tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(1139684315);
  return msg;
}

void
testcase(gchar *msg_string, gchar *template, gint padding, gboolean use_syslog_protocol,gchar *expected_value, gboolean local_message)
{
  LogTemplate *templ = NULL;
  LogWriter *writer = NULL;
  GString *res = g_string_sized_new(128);
  GError *error = NULL;
  LogMessage *msg = NULL;
  LogWriterOptions opt = {0};
  guint msg_flags = 0;
  guint writer_flags = 0;

  static TimeZoneInfo *tzinfo = NULL;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);
  opt.options = LWO_NO_MULTI_LINE | LWO_NO_STATS | LWO_SHARE_STATS;
  opt.template_options.time_zone_info[LTZ_SEND]=tzinfo;
  if (use_syslog_protocol)
    {
      opt.options |= LWO_SYSLOG_PROTOCOL;
      msg_flags |= LP_SYSLOG_PROTOCOL;
      writer_flags = LW_SYSLOG_PROTOCOL;
    }
  if (template)
    {
      templ = log_template_new(configuration, "dummy");
      log_template_compile(templ, template, &error);
    }
  if (padding)
    opt.padding = padding;
  opt.template = templ;
  msg = init_msg(msg_string, use_syslog_protocol, local_message);
  writer = (LogWriter*)log_writer_new(writer_flags);
  log_writer_set_queue((LogPipe *)writer, log_queue_fifo_new(1000, NULL));
  if (writer)
    {
      log_writer_set_options(writer, NULL, &opt, 0, 0, NULL, NULL, NULL);
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
    g_slice_free(LogMessage, msg);
  g_string_free(res, TRUE);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  char *msg_syslog_str = "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...";
  char *expected_msg_syslog_str = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...\n";
  char *expected_msg_syslog_str_t = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n";

  char *msg_syslog_empty_str ="<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]";
  char *expected_msg_syslog_empty_str ="<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \n";
  char *expected_msg_syslog_empty_str_t ="<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47\n";

  char *expected_syslog_msg = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n";
  char *expected_syslog_msg_seq_num = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"][meta sequenceId=\"1\"] ID47 BOMAn application event log entry...\n";

  char *expected_syslog_msg_seq_num_2 = "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"][meta sequenceId=\"12\"] ID47 BOMAn application event log entry...\n";

  char *msg_bsd_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  char *expected_msg_bsd_str = " bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";
  char *expected_msg_bsd_str_t = "23323 árvíztűrőtükörfúrógép";

  /*char *msg_zero_pri = "<0>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  char *expected_msg_zero_pri_str = "<0>Feb 11 10:34:56 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";
  char *expected_msg_zero_pri_str_t = "0";*/ /*not used*/

  char *msg_bsd_empty_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:";
  char *expected_msg_bsd_empty_str = " bzorp syslog-ng[23323]:\n";
  char *expected_msg_bsd_empty_str_t = "23323";

  char *expected_msg_bsd_empty_str_padd = " bzorp syslog-ng[23323]:\0\0\0\0\0\0$"; /* \\ is the last byte to check that the padding is not to big */
  char *expected_msg_bsd_empty_str_t_padd = "23323\0\0\0$";
  char *expected_msg_bsd_empty_str_t_n_padd = "23323\n\0\0\0$";

  if (argc > 1)
    verbose = TRUE;

  configuration = cfg_new(0x0303);
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);


  //Testing syslog protocol if template string is not used
  testcase(msg_syslog_str,NULL,0,TRUE,expected_msg_syslog_str, FALSE);
  //Testing syslog protocol if template string is used
  testcase(msg_syslog_str,"$MSGID $MSG",0,TRUE,expected_msg_syslog_str_t, FALSE);
  //Testing syslog protocol if template string is not used and $MSG is empty
  testcase(msg_syslog_empty_str,NULL,0,TRUE,expected_msg_syslog_empty_str, FALSE);
  //Testing syslog protocol if template string is used and $MSG is empty
  testcase(msg_syslog_empty_str,"$MSGID",0,TRUE,expected_msg_syslog_empty_str_t, FALSE);

  testcase(msg_bsd_str,NULL,0,FALSE,expected_msg_bsd_str, FALSE);
  testcase(msg_bsd_str,"$PID $MSG",0,FALSE,expected_msg_bsd_str_t, FALSE);
  testcase(msg_bsd_empty_str,NULL,0,FALSE,expected_msg_bsd_empty_str, FALSE);
  testcase(msg_bsd_empty_str,"$PID",0,FALSE,expected_msg_bsd_empty_str_t, FALSE);

  testcase(msg_bsd_empty_str,NULL,30,FALSE,expected_msg_bsd_empty_str_padd, FALSE);
  testcase(msg_bsd_empty_str,"$PID",8,FALSE,expected_msg_bsd_empty_str_t_padd, FALSE);
  testcase(msg_bsd_empty_str,"$PID\n",9,FALSE,expected_msg_bsd_empty_str_t_n_padd, FALSE);

  //Testing syslog protocol if squenceID is not specified in SDATA part of incoming message
  testcase("<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n",NULL,0,TRUE,expected_syslog_msg_seq_num,TRUE);
  //Testing syslog protocol if squenceID is specified in SDATA part of incoming message
  testcase("<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"][meta sequenceId=\"12\"] ID47 BOMAn application event log entry...\n",NULL,0,TRUE,expected_syslog_msg_seq_num_2,FALSE);

  testcase("<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [timeQuality isSynced=\"0\" tzKnown=\"1\"][exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n",NULL,0, TRUE,expected_syslog_msg,FALSE);

  app_shutdown();
  return 0;
}
