#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "timeutils.h"
#include "cfg.h"
#include "plugin.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT(x, format, value, expected)		\
  do 				\
    { 				\
        if (!(x))		\
          {			\
            fprintf(stderr, "Testcase failed; msg='%s', cond='%s', value=" format ", expected=" format "\n", msg, #x, value, expected); \
            exit(1);		\
          }			\
    }				\
  while (0)

MsgFormatOptions parse_options;

unsigned long
absolute_value(signed long diff)
{
  if (diff < 0)
    return -diff;
  else
    return diff;
}

void
check_sd_param(gchar *msg, LogMessage *self, const gchar* pair[2])
{
  const gchar *a = log_msg_get_value(self, log_msg_get_value_handle(pair[0]), NULL);

  TEST_ASSERT(strcmp(a, pair[1]) == 0, "%s", a, pair[1]);
}

void
check_sd_param_in_clone(gchar *msg, LogMessage *self, LogMessage *msg_clone, const gchar* pair[2])
{
  const gchar *a1 = log_msg_get_value(self, log_msg_get_value_handle(pair[0]), NULL);
  const gchar *a2 = log_msg_get_value(msg_clone, log_msg_get_value_handle(pair[0]), NULL);

  TEST_ASSERT(strcmp(a1, pair[1]) == 0 && strcmp(a2, pair[1]) == 0, "%s", a1, a2);
}

void
check_value(gchar *msg, LogMessage *logmsg, NVHandle handle, const gchar *expected)
{
  const gchar *p = log_msg_get_value(logmsg, handle, NULL);

  TEST_ASSERT(strcmp(p, expected) == 0, "%s", p, expected);
}

int
testcase(gchar *msg,
         gint parse_flags,
         gchar *bad_hostname_re,
         gint expected_pri,
         unsigned long expected_stamp_sec,
         unsigned long expected_stamp_usec,
         unsigned long expected_stamp_ofs,
         const gchar *expected_host,
         const gchar *expected_program,
         const gchar *expected_msg,
         const gchar *expected_sd_str,
         const gchar *expected_pid,
         const gchar *expected_msgid,
         const gchar *expected_sd_pairs[][2])
{
  LogMessage *logmsg;
  time_t now;
  regex_t bad_hostname;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  gchar logmsg_addr[256];
  gint i;
  GString *sd_str = g_string_sized_new(0);

  if (bad_hostname_re)
    TEST_ASSERT(regcomp(&bad_hostname, bad_hostname_re, REG_NOSUB | REG_EXTENDED) == 0, "%d", 0, 0);

  parse_options.flags = parse_flags;
  parse_options.bad_hostname = bad_hostname_re ? &bad_hostname : NULL;
  logmsg = log_msg_new(msg, strlen(msg), addr, &parse_options);

  /* NOTE: this if statement mimics what LogReader does when the message has no timezone information */
  if (logmsg->timestamps[LM_TS_STAMP].zone_offset == -1)
    logmsg->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(logmsg->timestamps[LM_TS_STAMP].tv_sec);

  TEST_ASSERT(logmsg->pri == expected_pri, "%d", logmsg->pri, expected_pri);
  if (expected_stamp_sec)
    {
      if (expected_stamp_sec != 1)
        {
          TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].tv_sec == expected_stamp_sec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].tv_sec, (int) expected_stamp_sec);
        }
      TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].tv_usec == expected_stamp_usec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].tv_usec, (int) expected_stamp_usec);
      TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].zone_offset == expected_stamp_ofs, "%d", (int) logmsg->timestamps[LM_TS_STAMP].zone_offset, (int) expected_stamp_ofs);
    }
  else
    {
      time(&now);
      TEST_ASSERT(absolute_value(logmsg->timestamps[LM_TS_STAMP].tv_sec - now) < 1, "%d", 0, 0);
    }
  check_value(msg, logmsg, LM_V_HOST, expected_host);
  check_value(msg, logmsg, LM_V_PROGRAM, expected_program);
  check_value(msg, logmsg, LM_V_MESSAGE, expected_msg);
  if (expected_pid)
    check_value(msg, logmsg, LM_V_PID, expected_pid);
  if (expected_msgid)
    check_value(msg, logmsg, LM_V_MSGID, expected_msgid);

  /* SD elements */
  log_msg_format_sdata(logmsg, sd_str);
  TEST_ASSERT(!expected_sd_str || strcmp(sd_str->str, expected_sd_str) == 0, "%s", sd_str->str, expected_sd_str);

  if (expected_sd_pairs)
    {
      for (i = 0; expected_sd_pairs[i][0] != NULL;i++)
        check_sd_param(msg, logmsg, expected_sd_pairs[i]);
    }

  g_sockaddr_format(logmsg->saddr, logmsg_addr, sizeof(logmsg_addr), GSA_FULL);

  log_msg_unref(logmsg);
  g_string_free(sd_str, TRUE);
  return 0;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           15, 			// pri
           0, 0, 0,		// timestamp (sec/usec/zone)
           "",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1163148201, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.15600000000000000000000000000000000000000000000000000000000000+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-11-10T10:43:21.15600000000 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-03-26T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1143334799, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-03-26T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1143334800, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-03-26T03:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1143334800, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162076400, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:59:59.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162079999, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T02:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162080000, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  /* the same in a foreign timezone */
  testcase("<7>2006-10-29T01:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162080000, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  /* check hostname */

  testcase("<7>2006-10-29T02:00:00.156+01:00 %bzorp openvpn[2499]: PTHREAD support initialized", LP_CHECK_HOSTNAME | LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "%bzorp",		// openvpn
           "openvpn[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );


  testcase("<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "bzorp",	// program
           "openvpn[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "",		// openvpn
           "", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "",		// openvpn
           "", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",		// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7> Aug 29 02:00:00.156 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7, 			// pri
           1, 156000, 7200,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00.156789 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7, 			// pri
           1, 156789, 7200,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00. ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "bzorp",	        // host
           "ctld/snmpd",	// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<190>Apr 15 2007 21:28:13: %PIX-6-302014: Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs", LP_EXPECT_HOSTNAME, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%PIX-6-302014",
           "Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs",
           NULL, NULL, NULL, NULL
           );

  testcase("<190>Apr 15 2007 21:28:13 %ASA: this is a Cisco ASA timestamp", LP_EXPECT_HOSTNAME, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%ASA",
           "this is a Cisco ASA timestamp",
           NULL, NULL, NULL, NULL
           );
  testcase("<190>Apr 15 21:28:13 2007 linksys app: msg", LP_EXPECT_HOSTNAME, NULL,
           190,
           1176665293, 0, 7200,
           "linksys",
           "app",
           "msg",
           NULL, NULL, NULL, NULL
           );

  const gchar *expected_sd_pairs_test_1[][2]=
  {
    { ".SDATA.exampleSDID@0.iut", "3"},
    { ".SDATA.exampleSDID@0.eventSource", "Application"},
    { ".SDATA.exampleSDID@0.eventID", "1011"},
    { ".SDATA.examplePriority@0.class", "high"},
    {  NULL , NULL}
  };

  testcase("<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...", LP_SYSLOG_PROTOCOL, NULL,
           7, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine.example.com",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           7, 			// pri
           1162087199, 156000, 0,	// timestamp (sec/usec/zone)
           "mymachine.example.com",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156123Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           7, 			// pri
           1162087199, 156123, 0,	// timestamp (sec/usec/zone)
           "mymachine.example.com",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43, 			// pri
           0, 0, 0,	// timestamp (sec/usec/zone)
           "",		// host
           "syslog-ng", //app
           "Error processing log message: <7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...", // msg
           "",
           NULL,//processid
           NULL,//msgid
           NULL
           );

  testcase("<34>1 1987-01-01T12:00:27.000087+00:20 192.0.2.1 myproc 8710 - - %% It's time to make the do-nuts.", LP_SYSLOG_PROTOCOL, NULL,
           34,
           536499627, 87, 1200,
           "192.0.2.1",
           "myproc",
           "%% It's time to make the do-nuts.",
           "",
           "8710",
           "",
           NULL);


  const gchar *expected_sd_pairs_test_2[][2]=
  {
    { ".SDATA.exampleSDID@0.iut", "3"},
    {  NULL , NULL}
  };

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\"] [eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "[eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\"]", //sd_str
           "",//processid
           "",//msgid
           expected_sd_pairs_test_2
           );

  testcase("<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]:", LP_EXPECT_HOSTNAME, NULL,
           7,           // pri
           1, 0, 7200,          // timestamp (sec/usec/zone)
           "bzorp",         // host
           "ctld/snmpd",    // openvpn
           "", // msg
           NULL, "2499", NULL, NULL
           );

  const gchar *expected_sd_pairs_test_3[][2]=
  {
    { ".SDATA.origin.ip", "exchange.macartney.esbjerg"},
    { ".SDATA.meta.sequenceId", "191732"},
    { ".SDATA.EventData@18372.4.Data", "MSEXCHANGEOWAAPPPOOL.CONFIG\" -W \"\" -M 1 -AP \"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 " },
    {  NULL , NULL}
  };

  testcase("<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"] ApplicationMSExchangeADAccess: message",
           LP_SYSLOG_PROTOCOL, NULL,
           134, 			// pri
           1255686716, 0, 7200,	// timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",		// host
           "MSExchange_ADAccess", //app
           "ApplicationMSExchangeADAccess: message", // msg
           "[origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"]", //sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_3
           );

  const gchar *expected_sd_pairs_test_4[][2]=
  {
    { ".SDATA.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.i", "ok_32"},
    {  NULL , NULL}
  };

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"ok_32\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"ok_32\"]", //sd_str
           "",//processid
           "",//msgid
           expected_sd_pairs_test_4
           );

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,                         // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "",         // host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"long_33\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           0
           );

   const gchar *expected_sd_pairs_test_5[][2]=
     {
       { ".SDATA.a.i", "]\"\\"},
       {  NULL , NULL}
    };

   testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\\]\\\"\\\\\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"\\]\\\"\\\\\"]", //sd_str
           "",//processid
           "",//msgid
           expected_sd_pairs_test_5
           );

 // failed to parse to long sd name
  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43, 			// pri
           0, 0, 0,	// timestamp (sec/usec/zone)
           "",		// host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\"long_33\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           0
           );

   const gchar *expected_sd_pairs_test_5b[][2]=
     {
       { ".SDATA.a.i", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
       {  NULL , NULL}
    };

  // too long sdata value gets truncated

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]", //sd_str
           "",//processid
           "",//msgid
           expected_sd_pairs_test_5b
           );

  const gchar *expected_sd_pairs_test_6[][2]=
    {
       { ".SDATA.a.i", "ok"},
       {  NULL , NULL}
    };

// too long hostname
   testcase("<132>1 2006-10-29T01:59:59.156+01:00 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa evntslog - - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,		// pri
           0, 0, 0,	// timestamp (sec/usec/zone)
           "", //host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa evntslog - - [a i=\"ok\"] An application event log entry...",		// msg
           "", //sd_str
           0,//processid
           0,//msgid
           0
           );

 // failed to parse to long appname
  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           0,//processid
           0,//msgid
           expected_sd_pairs_test_6
           );
 // failed to parse to long procid

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//processid
           0,//msgid
           expected_sd_pairs_test_6
           );

 // failed to parse to long msgid

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132, 	// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "mymachine",		// host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           0,//processid
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//msgid
           expected_sd_pairs_test_6
           );
// unescaped ]

testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"]ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43, 			// pri
           0, 0, 0,	// timestamp (sec/usec/zone)
           "",		// host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"]ok\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           0
           );
// unescaped '\'

// bad sd data unescaped "
 testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43, 			// pri
           0, 0, 0,	// timestamp (sec/usec/zone)
           "",		// host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           0
           );

  const gchar *expected_sd_pairs_test_7[][2]=
  {
    { ".SDATA.origin.ip", "exchange.macartney.esbjerg"},
    { ".SDATA.meta.sequenceId", "191732"},
    { ".SDATA.EventData@18372.4.Data", "MSEXCHANGEOWAAPPPOOL.CONFIG\" -W \"\" -M 1 -AP \"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 " },
    {  NULL , NULL}
  };

  testcase("<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"] ApplicationMSExchangeADAccess: message",
           LP_SYSLOG_PROTOCOL, NULL,
           134, 			// pri
           1255686716, 0, 7200,	// timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",		// host
           "MSExchange_ADAccess", //app
           "ApplicationMSExchangeADAccess: message", // msg
           "[origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"]", //sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_7
           );


  app_shutdown();
  return 0;
}


