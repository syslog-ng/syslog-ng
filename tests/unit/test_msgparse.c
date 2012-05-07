#include "testutils.h"
#include "msg_parse_lib.h"

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

unsigned long
absolute_value(signed long diff)
{
  if (diff < 0)
    return -diff;
  else
    return diff;
}

/* This function determines the year that syslog-ng would find out
 * given the timestamp has no year information. Then returns the UTC
 * representation of "January 1st 00:00:00" of that year. This is to
 * be used for testcases that lack year information. ts_month is the 0
 * based month in the timestamp being parsed.
 */
time_t
get_bsd_year_utc(int ts_month)
{
  struct tm *tm;
  time_t t;

  time(&t);
  tm = localtime(&t);

  if (tm->tm_mon > ts_month + 1)
    tm->tm_year++;

  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  tm->tm_mday = 1;
  tm->tm_mon = 0;
  tm->tm_isdst = -1;
  return mktime(tm);
}

void
assert_log_message_sdata_pairs(LogMessage *message, const gchar *expected_sd_pairs[][2])
{
  gint i;
  for (i = 0; expected_sd_pairs[i][0] != NULL;i++)
    {
      const gchar *actual_value = log_msg_get_value(message, log_msg_get_value_handle(expected_sd_pairs[i][0]), NULL);
      assert_string(actual_value, expected_sd_pairs[i][1], NULL);
    }
}

void
simulate_log_readers_effect_on_timezone_offset(LogMessage *message)
{
  if (message->timestamps[LM_TS_STAMP].zone_offset == -1)
    message->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(message->timestamps[LM_TS_STAMP].tv_sec);
}

LogMessage *
parse_log_message(gchar *raw_message_str, gint parse_flags, gchar *bad_hostname_re)
{
  LogMessage *message;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  regex_t bad_hostname;

  parse_options.flags = parse_flags;

  if (bad_hostname_re)
    {
      assert_gint(regcomp(&bad_hostname, bad_hostname_re, REG_NOSUB | REG_EXTENDED), 0,
                  "Unexpected failure of regcomp(); bad_hostname_re='%s'", bad_hostname_re);
      parse_options.bad_hostname = &bad_hostname;
    }

  message = log_msg_new(raw_message_str, strlen(raw_message_str), addr, &parse_options);

  if (bad_hostname_re)
    {
      regfree(parse_options.bad_hostname);
      parse_options.bad_hostname = NULL;
    }

  simulate_log_readers_effect_on_timezone_offset(message);
  return message;
}

void
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
  LogMessage *parsed_message;
  LogStamp *parsed_timestamp;
  time_t now;
  GString *sd_str;

  testcase_begin("Testing log message parsing; parse_flags='%x', bad_hostname_re='%s', msg='%s'",
                 parse_flags, bad_hostname_re ? : "(null)", msg);

  parsed_message = parse_log_message(msg, parse_flags, bad_hostname_re);
  parsed_timestamp = &(parsed_message->timestamps[LM_TS_STAMP]);

  if (expected_stamp_sec)
    {
      if (expected_stamp_sec != 1)
        assert_guint(parsed_timestamp->tv_sec, expected_stamp_sec, "Unexpected timestamp");
      assert_guint32(parsed_timestamp->tv_usec, expected_stamp_usec, "Unexpected microseconds");
      assert_guint32(parsed_timestamp->zone_offset, expected_stamp_ofs, "Unexpected timezone offset");
    }
  else
    {
      time(&now);
      assert_true(absolute_value(parsed_timestamp->tv_sec - now) <= 5,
                  "Expected parsed message timestamp to be set to now; now='%d', timestamp->tv_sec='%d'",
                  (gint)now, (gint)parsed_timestamp->tv_sec, NULL);
    }
  assert_guint16(parsed_message->pri, expected_pri, "Unexpected message priority");
  assert_log_message_value(parsed_message, LM_V_HOST, expected_host);
  assert_log_message_value(parsed_message, LM_V_PROGRAM, expected_program);
  assert_log_message_value(parsed_message, LM_V_MESSAGE, expected_msg);
  if (expected_pid)
    assert_log_message_value(parsed_message, LM_V_PID, expected_pid);
  if (expected_msgid)
    assert_log_message_value(parsed_message, LM_V_MSGID, expected_msgid);
  if (expected_sd_str)
    {
      sd_str = g_string_sized_new(0);
      log_msg_format_sdata(parsed_message, sd_str, 0);
      assert_string(sd_str->str, expected_sd_str, "Unexpected formatted SData");
      g_string_free(sd_str, TRUE);
    }

  assert_log_message_sdata_pairs(parsed_message, expected_sd_pairs);

  log_msg_unref(parsed_message);

  testcase_end();
}

void
test_log_messages_can_be_parsed()
{
  const gchar *ignore_sdata_pairs[][2] = { { NULL, NULL } };
  const gchar *empty_sdata_pairs[][2] = { { NULL, NULL } };

 // failed to parse too long sd id
  testcase("<5>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [timeQuality isSynced=\"0\"][1234567890123456789012345678901234 i=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,        //pri
      0, 0, 0,  // timestamp (sec/usec/zone)
      "", //host
      "syslog-ng", //app
      "Error processing log message: <5>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [timeQuality isSynced=\"0\"][1234567890123456789012345678901234 i=\"long_33\"] An application event log entry...", // msg
      "", // sd str,
      0, // processid
      0, // msgid,
      empty_sdata_pairs
      );

// bad sd data unescaped "
 testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,             // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "",        // host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           empty_sdata_pairs
           );

  testcase("<15> openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           15,             // pri
           0, 0, 0,        // timestamp (sec/usec/zone)
           "",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<15>Jan  1 01:00:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           15,             // pri
           get_bsd_year_utc(0) + 3600, 0, 3600,        // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<15>Jan 10 01:00:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           15,             // pri
           get_bsd_year_utc(0) + 3600 + 9 * 24 * 3600, 0, 3600,        // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<13>Jan  1 14:40:51 alma korte: message", 0, NULL,
       13,
       get_bsd_year_utc(0) + 60 * 60 * 14 + 40 * 60 + 51, 0, 3600,
       "",
       "alma",
       "korte: message",
       NULL, NULL, NULL, ignore_sdata_pairs
       );

  testcase("<7>2006-11-10T10:43:21.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1163148201, 156000, 7200,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-11-10T10:43:21.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-11-10T10:43:21.15600000000000000000000000000000000000000000000000000000000000+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-11-10T10:43:21.15600000000 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-03-26T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1143334799, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-03-26T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1143334800, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-03-26T03:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1143334800, 156000, 7200,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-10-29T01:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162076400, 156000, 7200,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-10-29T01:59:59.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162079999, 156000, 7200,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-10-29T02:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162080000, 156000, 7200,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  /* the same in a foreign timezone */
  testcase("<7>2006-10-29T01:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162080000, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-10-29T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "bzorp",        // host
           "openvpn",        // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );

  /* check hostname */

  testcase("<7>2006-10-29T02:00:00.156+01:00 %bzorp openvpn[2499]: PTHREAD support initialized", LP_CHECK_HOSTNAME | LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "",                // host
           "%bzorp",        // openvpn
           "openvpn[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );


  testcase("<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "",                // host
           "bzorp",    // program
           "openvpn[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "",                // host
           "",        // openvpn
           "", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "",                // host
           "",        // openvpn
           "", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<7>2006-10-29T02:00:00.156+01:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7,             // pri
           1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
           "",                // host
           "ctld",        // openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<7> Aug 29 02:00:00.156 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7,             // pri
           1, 156000, 7200,    // timestamp (sec/usec/zone)
           "",                // host
           "ctld",    // openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );
  testcase("<7> Aug 29 02:00:00.156789 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7,             // pri
           1, 156789, 7200,    // timestamp (sec/usec/zone)
           "",                // host
           "ctld",    // openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );
  testcase("<7> Aug 29 02:00:00. ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7,             // pri
           1, 0, 7200,            // timestamp (sec/usec/zone)
           "",                // host
           "ctld",    // openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );
  testcase("<7> Aug 29 02:00:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
           7,             // pri
           1, 0, 7200,            // timestamp (sec/usec/zone)
           "",                // host
           "ctld",    // openvpn
           "snmpd[2499]: PTHREAD support initialized", // msg
           NULL, NULL, NULL, ignore_sdata_pairs
           );
  testcase("<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
           7,             // pri
           1, 0, 7200,            // timestamp (sec/usec/zone)
           "bzorp",            // host
           "ctld/snmpd",    // openvpn
           "PTHREAD support initialized", // msg
           NULL, "2499", NULL, ignore_sdata_pairs
           );
  testcase("<190>Apr 15 2007 21:28:13: %PIX-6-302014: Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs", LP_EXPECT_HOSTNAME, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%PIX-6-302014",
           "Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs",
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<190>Apr 15 2007 21:28:13 %ASA: this is a Cisco ASA timestamp", LP_EXPECT_HOSTNAME, "^%",
           190,
           1176665293, 0, 7200,
           "",
           "%ASA",
           "this is a Cisco ASA timestamp",
           NULL, NULL, NULL, ignore_sdata_pairs
           );
  testcase("<190>Apr 15 21:28:13 2007 linksys app: msg", LP_EXPECT_HOSTNAME, NULL,
           190,
           1176665293, 0, 7200,
           "linksys",
           "app",
           "msg",
           NULL, NULL, NULL, ignore_sdata_pairs
           );

  testcase("<38>Sep 22 10:11:56 Message forwarded from cdaix66: sshd[679960]: Accepted publickey for nagios from 1.9.1.1 port 42096 ssh2", LP_EXPECT_HOSTNAME, NULL,
           38,
           1, 0, 7200,
           "cdaix66",
           "sshd",
           "Accepted publickey for nagios from 1.9.1.1 port 42096 ssh2",
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
           7,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine.example.com",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           7,             // pri
           1162087199, 156000, 0,    // timestamp (sec/usec/zone)
           "mymachine.example.com",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156123Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           7,             // pri
           1162087199, 156123, 0,    // timestamp (sec/usec/zone)
           "mymachine.example.com",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
           "",//processid
           "ID47",//msgid
           expected_sd_pairs_test_1
           );

  testcase("<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,             // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "",        // host
           "syslog-ng", //app
           "Error processing log message: <7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...", // msg
           "",
           NULL,//processid
           NULL,//msgid
           empty_sdata_pairs
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
           ignore_sdata_pairs);


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
           NULL, "2499", NULL, ignore_sdata_pairs
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
           134,             // pri
           1255686716, 0, 7200,    // timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",        // host
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
           132,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
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
           empty_sdata_pairs
           );

   const gchar *expected_sd_pairs_test_5[][2]=
     {
       { ".SDATA.a.i", "]\"\\"},
       {  NULL , NULL}
    };

   testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\\]\\\"\\\\\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"\\]\\\"\\\\\"]", //sd_str
           "",//processid
           "",//msgid
           expected_sd_pairs_test_5
           );

 // failed to parse to long sd name
  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,             // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "",        // host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\"long_33\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           empty_sdata_pairs
           );

   const gchar *expected_sd_pairs_test_5b[][2]=
     {
       { ".SDATA.a.i", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
       {  NULL , NULL}
    };

  // too long sdata value gets truncated

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
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
           43,        // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "", //host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa evntslog - - [a i=\"ok\"] An application event log entry...",        // msg
           "", //sd_str
           0,//processid
           0,//msgid
           empty_sdata_pairs
           );

 // failed to parse to long appname
  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           0,//processid
           0,//msgid
           expected_sd_pairs_test_6
           );
 // failed to parse to long procid

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132,             // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//processid
           0,//msgid
           expected_sd_pairs_test_6
           );

 // failed to parse to long msgid

  testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           132,     // pri
           1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
           "mymachine",        // host
           "evntslog", //app
           "An application event log entry...", // msg
           "[a i=\"ok\"]", //sd_str
           0,//processid
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//msgid
           expected_sd_pairs_test_6
           );
// unescaped ]

testcase("<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"]ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
           43,             // pri
           0, 0, 0,    // timestamp (sec/usec/zone)
           "",        // host
           "syslog-ng", //app
           "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"]ok\"] An application event log entry...", // msg
           "", //sd_str
           0,//processid
           0,//msgid
           empty_sdata_pairs
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
           134,             // pri
           1255686716, 0, 7200,    // timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",        // host
           "MSExchange_ADAccess", //app
           "ApplicationMSExchangeADAccess: message", // msg
           "[origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"]", //sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_7
           );

/*###########################x*/
  const gchar *expected_sd_pairs_test_7a[][2]=
  {
    /*{ ".SDATA.timeQuality.isSynced", "0"},
    { ".SDATA.timeQuality.tzKnown", "0"},*/
    {  NULL , NULL}
  };

  //Testing syslog protocol message parsing if tzKnown=0 because there is no timezone information
  testcase("<134>1 2009-10-16T11:51:56 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - - An application event log entry...",
           LP_SYSLOG_PROTOCOL, NULL,
           134,                         // pri
           1255686716, 0, 7200, // timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",                // host
           "MSExchange_ADAccess", //app
           "An application event log entry...", // msg
           "",//sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_7a
           );

  const gchar *expected_sd_pairs_test_8[][2]=
  {
/*    { ".SDATA.timeQuality.isSynced", "0"},
    { ".SDATA.timeQuality.tzKnown", "1"},*/
    { ".SDATA.origin.enterpriseId", "1.3.6.1.4.1"},
    {  NULL , NULL}
  };

  //Testing syslog protocol message parsing if SDATA contains origin enterpriseID
  testcase("<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin enterpriseId=\"1.3.6.1.4.1\"] An application event log entry...",
           LP_SYSLOG_PROTOCOL, NULL,
           134,                         // pri
           1255686716, 0, 7200, // timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",                // host
           "MSExchange_ADAccess", //app
           "An application event log entry...", // msg
           "[origin enterpriseId=\"1.3.6.1.4.1\"]",//sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_8
           );

  //Testing syslog protocol message parsing if size of origin software and swVersion are longer than the maximum size (48 and 32)
  //KNOWN BUG: 22045
/*  testcase("<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin software=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\" swVersion=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"] An application event log entry...",
           LP_SYSLOG_PROTOCOL, NULL,
           43,                         // pri
           0, 0, 0, // timestamp (sec/usec/zone)
           "",                // host
           "syslog-ng", //app
           "Error processing log message: <134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin software=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\" swVersion=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"] An application event log entry...", // msg
           "",
           "0",//processid
           "0",//msgid
           0
           );*/

  const gchar *expected_sd_pairs_test_9[][2]=
  {
/*    { ".SDATA.timeQuality.isSynced", "0"},
    { ".SDATA.timeQuality.tzKnown", "1"},*/
    { ".SDATA.origin.enterpriseId", "1.3.6.1.4.1"},
    {  NULL , NULL}
  };

  //Testing syslog protocol message parsing if SDATA contains only SD-ID without SD-PARAM
  //KNOWN BUG: 20459
  testcase("<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin enterpriseId=\"1.3.6.1.4.1\"][nosdnvpair] An application event log entry...",
           LP_SYSLOG_PROTOCOL, NULL,
           134,                         // pri
           1255686716, 0, 7200, // timestamp (sec/usec/zone)
           "exchange.macartney.esbjerg",                // host
           "MSExchange_ADAccess", //app
           "An application event log entry...", // msg
           "[origin enterpriseId=\"1.3.6.1.4.1\"][nosdnvpair]",//sd_str
           "20208",//processid
           "",//msgid
           expected_sd_pairs_test_9
           );


/*############################*/
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();
  init_and_load_syslogformat_module();

  test_log_messages_can_be_parsed();

  deinit_syslogformat_module();
  app_shutdown();
  return 0;
}


