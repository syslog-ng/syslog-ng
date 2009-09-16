#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "timeutils.h"

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
  const gchar *a = log_msg_lookup_sdata(self, pair[0], -1);

  TEST_ASSERT(strcmp(a, pair[1]) == 0, "%s", a, pair[1]);
}

void
check_sd_param_in_clone(gchar *msg, LogMessage *self, LogMessage *msg_clone, const gchar* pair[2])
{
  const gchar *a1 = log_msg_lookup_sdata(self, pair[0], -1);
  const gchar *a2 = log_msg_lookup_sdata(msg_clone, pair[0], -1);

  TEST_ASSERT(strcmp(a1, pair[1]) == 0 && strcmp(a2, pair[1]) == 0, "%s", a1, a2);
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

  logmsg = log_msg_new(msg, strlen(msg), addr, parse_flags, bad_hostname_re ? &bad_hostname : NULL, -1, 0xFFFF);

  /* NOTE: this if statement mimics what LogReader does when the message has no timezone information */
  if (logmsg->timestamps[LM_TS_STAMP].zone_offset == -1)
    logmsg->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(logmsg->timestamps[LM_TS_STAMP].time.tv_sec);

  TEST_ASSERT(logmsg->pri == expected_pri, "%d", logmsg->pri, expected_pri);
  if (expected_stamp_sec)
    {
      if (expected_stamp_sec != 1)
        {
          TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_sec == expected_stamp_sec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_sec, (int) expected_stamp_sec);
        }
      TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].time.tv_usec == expected_stamp_usec, "%d", (int) logmsg->timestamps[LM_TS_STAMP].time.tv_usec, (int) expected_stamp_usec);
      TEST_ASSERT(logmsg->timestamps[LM_TS_STAMP].zone_offset == expected_stamp_ofs, "%d", (int) logmsg->timestamps[LM_TS_STAMP].zone_offset, (int) expected_stamp_ofs);
    }
  else
    {
      time(&now);
      TEST_ASSERT(absolute_value(logmsg->timestamps[LM_TS_STAMP].time.tv_sec - now) < 1, "%d", 0, 0);
    }
  TEST_ASSERT(strcmp(logmsg->host, expected_host) == 0, "%s", logmsg->host, expected_host);
  TEST_ASSERT(strcmp(logmsg->program, expected_program) == 0, "%s", logmsg->program, expected_program);
  TEST_ASSERT(strcmp(logmsg->message, expected_msg) == 0, "%s", logmsg->message, expected_msg);
  TEST_ASSERT(!expected_pid || strcmp(logmsg->pid, expected_pid) == 0, "%s", logmsg->pid, expected_pid);
  TEST_ASSERT(!expected_msgid || strcmp(logmsg->msgid, expected_msgid) == 0, "%s", logmsg->msgid, expected_msgid);

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
  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, NULL,
           15, 			// pri
           0, 0, 0,		// timestamp (sec/usec/zone)
           "",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1163148201, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-11-10T10:43:21.15600000000000000000000000000000000000000000000000000000000000+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-11-10T10:43:21.15600000000 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1163151801, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-03-26T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1143334799, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  testcase("<7>2006-03-26T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1143334800, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-03-26T03:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1143334800, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162076400, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:59:59.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162079999, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T02:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162080000, 156000, 7200,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  /* the same in a foreign timezone */
  testcase("<7>2006-10-29T01:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162080000, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162083599, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "bzorp",		// host
           "openvpn",		// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );

  /* check hostname */

  testcase("<7>2006-10-29T02:00:00.156+01:00 %bzorp openvpn[2499]: PTHREAD support initialized", LP_CHECK_HOSTNAME, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "%bzorp",		// openvpn
           "openvpn[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ", 0, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "",		// openvpn
           "", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00", 0, NULL,
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",	        	// host
           "",		// openvpn
           "", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ctld snmpd[2499]: PTHREAD support initialized", 0, "^ctld",
           7, 			// pri
           1162083600, 156000, 3600,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",		// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );

  testcase("<7> Aug 29 02:00:00.156 ctld snmpd[2499]: PTHREAD support initialized", 0, "^ctld",
           7, 			// pri
           1, 156000, 7200,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00.156789 ctld snmpd[2499]: PTHREAD support initialized", 0, "^ctld",
           7, 			// pri
           1, 156789, 7200,	// timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00. ctld snmpd[2499]: PTHREAD support initialized", 0, "^ctld",
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7> Aug 29 02:00:00 ctld snmpd[2499]: PTHREAD support initialized", 0, "^ctld",
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "",		        // host
           "ctld",	// openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, NULL
           );
  testcase("<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]: PTHREAD support initialized", 0, NULL,
           7, 			// pri
           1, 0, 7200,	        // timestamp (sec/usec/zone)
           "bzorp",	        // host
           "ctld/snmpd",	// openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, NULL
           );
  testcase("<190>Apr 15 2007 21:28:13: %PIX-6-302014: Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs", 0, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%PIX-6-302014",
           "Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs",
           NULL, NULL, NULL, NULL
           );

  testcase("<190>Apr 15 2007 21:28:13 %ASA: this is a Cisco ASA timestamp", 0, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%ASA",
           "this is a Cisco ASA timestamp",
           NULL, NULL, NULL, NULL
           );
  testcase("<190>Apr 15 21:28:13 2007 linksys app: msg", 0, NULL,
           190,
           1176665293, 0, 7200,
           "linksys",
           "app",
           "msg",
           NULL, NULL, NULL, NULL
           );

  const gchar *expected_sd_pairs_test_1[][2]=
  {
    { "exampleSDID@0.iut", "3"},
    { "exampleSDID@0.eventSource", "Application"},
    { "exampleSDID@0.eventID", "1011"},
    { "examplePriority@0.class", "high"},
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
    { "exampleSDID@0.iut", "3"},
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

  app_shutdown();
  return 0;
}


