/*
 * Copyright (c) 2007-2016 Balabit
 * Copyright (c) 2007-2015 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "msg_parse_lib.h"

#include "syslog-ng.h"
#include "logmsg/logmsg.h"
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

struct sdata_pair
{
  const gchar *name;
  const gchar *value;
};

struct sdata_pair ignore_sdata_pairs[] = { { NULL, NULL } };
struct sdata_pair empty_sdata_pairs[] = { { NULL, NULL } };

static unsigned long
_absolute_value(signed long diff)
{
  if (diff < 0)
    return -diff;
  else
    return diff;
}

static time_t
_get_epoch_with_bsd_year(int ts_month, int d, int h, int m, int s)
{
  struct tm *tm;
  time_t t;

  time(&t);
  tm = localtime(&t);

  tm->tm_year = determine_year_for_month(ts_month, tm);

  tm->tm_hour = h;
  tm->tm_min = m;
  tm->tm_sec = s;
  tm->tm_mday = d;
  tm->tm_mon = ts_month;
  tm->tm_isdst = -1;
  return mktime(tm);
}


static void
_simulate_log_readers_effect_on_timezone_offset(LogMessage *message)
{
  if (message->timestamps[LM_TS_STAMP].zone_offset == -1)
    message->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(message->timestamps[LM_TS_STAMP].tv_sec);
}

static LogMessage *
_parse_log_message(gchar *raw_message_str, gint parse_flags, gchar *bad_hostname_re)
{
  LogMessage *message;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  regex_t bad_hostname;

  parse_options.flags = parse_flags;
  parse_options.sdata_param_value_max = 255;

  if (bad_hostname_re)
    {
      cr_assert_eq(regcomp(&bad_hostname, bad_hostname_re, REG_NOSUB | REG_EXTENDED), 0,
                   "Unexpected failure of regcomp(); bad_hostname_re='%s'", bad_hostname_re);
      parse_options.bad_hostname = &bad_hostname;
    }

  message = log_msg_new(raw_message_str, strlen(raw_message_str), addr, &parse_options);

  if (bad_hostname_re)
    {
      regfree(parse_options.bad_hostname);
      parse_options.bad_hostname = NULL;
    }

  _simulate_log_readers_effect_on_timezone_offset(message);
  return message;
}

void
assert_log_message_sdata_pairs(LogMessage *message, struct sdata_pair *expected_sd_pairs)
{
  gint i;
  for (i = 0; expected_sd_pairs && expected_sd_pairs[i].name != NULL; i++)
    {
      const gchar *actual_value = log_msg_get_value_by_name(message, expected_sd_pairs[i].name, NULL);
      cr_assert_str_eq(actual_value, expected_sd_pairs[i].value);
    }
}


struct msgparse_params
{
  gchar *msg;
  gint parse_flags;
  gchar *bad_hostname_re;
  gint expected_pri;
  unsigned long expected_stamp_sec;
  unsigned long expected_stamp_usec;
  unsigned long expected_stamp_ofs;
  const gchar *expected_host;
  const gchar *expected_program;
  const gchar *expected_msg;
  const gchar *expected_sd_str;
  const gchar *expected_pid;
  const gchar *expected_msgid;
  struct sdata_pair *expected_sd_pairs;
};

void
setup(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();
  init_and_load_syslogformat_module();
}

void
teardown(void)
{
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(msgparse, .init = setup, .fini = teardown);

void
test_log_messages_can_be_parsed(struct msgparse_params *param)
{
  LogMessage *parsed_message;
  LogStamp *parsed_timestamp;
  time_t now;
  GString *sd_str;

  parsed_message = _parse_log_message(param->msg, param->parse_flags, param->bad_hostname_re);
  parsed_timestamp = &(parsed_message->timestamps[LM_TS_STAMP]);

  if (param->expected_stamp_sec)
    {
      if (param->expected_stamp_sec != 1)
        cr_assert_eq(parsed_timestamp->tv_sec, param->expected_stamp_sec,
                     "Unexpected timestamp, value=%ld, expected=%ld, msg=%s",
                     parsed_timestamp->tv_sec, param->expected_stamp_sec, param->msg);

      cr_assert_eq(parsed_timestamp->tv_usec, param->expected_stamp_usec, "Unexpected microseconds");
      cr_assert_eq(parsed_timestamp->zone_offset, param->expected_stamp_ofs, "Unexpected timezone offset");
    }
  else
    {
      time(&now);
      cr_assert(_absolute_value(parsed_timestamp->tv_sec - now) <= 5,
                "Expected parsed message timestamp to be set to now; now='%d', timestamp->tv_sec='%d'",
                (gint)now, (gint)parsed_timestamp->tv_sec);
    }

  cr_assert_eq(parsed_message->pri, param->expected_pri, "Unexpected message priority");
  assert_log_message_value(parsed_message, LM_V_HOST, param->expected_host);
  assert_log_message_value(parsed_message, LM_V_PROGRAM, param->expected_program);
  assert_log_message_value(parsed_message, LM_V_MESSAGE, param->expected_msg);
  if (param->expected_pid)
    assert_log_message_value(parsed_message, LM_V_PID, param->expected_pid);
  if (param->expected_msgid)
    assert_log_message_value(parsed_message, LM_V_MSGID, param->expected_msgid);
  if (param->expected_sd_str)
    {
      sd_str = g_string_sized_new(0);
      log_msg_format_sdata(parsed_message, sd_str, 0);
      cr_assert_str_eq(sd_str->str, param->expected_sd_str, "Unexpected formatted SData");
      g_string_free(sd_str, TRUE);
    }

  assert_log_message_sdata_pairs(parsed_message, param->expected_sd_pairs);

  log_msg_unref(parsed_message);
}

void
run_parameterized_test(struct msgparse_params *params)
{
  struct msgparse_params *param = params;

  for (; param->msg != NULL; ++param)
    test_log_messages_can_be_parsed(param);
}

Test(msgparse, test_failed_to_parse_too_long_sd_id)
{
  struct msgparse_params params[] =
  {
    {
      "<5>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [timeQuality isSynced=\"0\"][1234567890123456789012345678901234 i=\"long_33\"] An application event log entry...",
      LP_SYSLOG_PROTOCOL, NULL,
      43,        //pri
      0, 0, 0,  // timestamp (sec/usec/zone)
      "", //host
      "syslog-ng", //app
      "Error processing log message: <5>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [timeQuality isSynced=\"0\"][1234567890123456789012345678901>@<234 i=\"long_33\"] An application event log entry...", // msg
      "", // sd str,
      0, // processid
      0, // msgid,
      empty_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_bad_sd_data_unescaped)
{
  struct msgparse_params params[] =
  {
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...",
      LP_SYSLOG_PROTOCOL, NULL,
      43,             // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "",        // host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\">@<\"ok\"] An application event log entry...", // msg
      "", //sd_str
      0,//processid
      0,//msgid
      empty_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_timestamp)
{
  struct msgparse_params params[] =
  {
    {
      "<15> openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      15,             // pri
      0, 0, 0,        // timestamp (sec/usec/zone)
      "",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<15>Jan  1 01:00:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      15,             // pri
      _get_epoch_with_bsd_year(0, 1, 1, 0, 0), 0, 3600,        // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<15>Jan 10 01:00:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      15,             // pri
      _get_epoch_with_bsd_year(0, 10, 1, 0, 0)
      , 0, 3600,        // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<13>Jan  1 14:40:51 alma korte: message", 0, NULL,
      13,
      _get_epoch_with_bsd_year(0, 1, 14, 40, 51), 0, 3600,
      "",
      "alma",
      "korte: message",
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-11-10T10:43:21.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1163148201, 156000, 7200,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-11-10T10:43:21.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-11-10T10:43:21.15600000000000000000000000000000000000000000000000000000000000+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-11-10T10:43:21.15600000000 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1163151801, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-03-26T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1143334799, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-03-26T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1143334800, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-03-26T03:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1143334800, 156000, 7200,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T01:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162076400, 156000, 7200,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T01:59:59.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162079999, 156000, 7200,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T02:00:00.156+02:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162080000, 156000, 7200,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_foreign_timezone)
{
  struct msgparse_params params[] =
  {
    {
      "<7>2006-10-29T01:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162080000, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T01:59:59.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "bzorp",        // host
      "openvpn",        // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_hostname)
{
  struct msgparse_params params[] =
  {
    {
      "<7>2006-10-29T02:00:00.156+01:00 %bzorp openvpn[2499]: PTHREAD support initialized", LP_CHECK_HOSTNAME | LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "",                // host
      "%bzorp",        // openvpn
      "openvpn[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_timestamp_others)
{
  struct msgparse_params params[] =
  {
    {
      "<7>2006-10-29T02:00:00.156+01:00 bzorp openvpn[2499]: PTHREAD support initialized", 0, NULL,
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "",                // host
      "bzorp",    // program
      "openvpn[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T02:00:00.156+01:00 ", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "",                // host
      "",        // openvpn
      "", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T02:00:00.156+01:00", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "",                // host
      "",        // openvpn
      "", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7>2006-10-29T02:00:00.156+01:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
      7,             // pri
      1162083600, 156000, 3600,    // timestamp (sec/usec/zone)
      "",                // host
      "ctld",        // openvpn
      "snmpd[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7> Aug 29 02:00:00.156 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
      7,             // pri
      1, 156000, 7200,    // timestamp (sec/usec/zone)
      "",                // host
      "ctld",    // openvpn
      "snmpd[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7> Aug 29 02:00:00.156789 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
      7,             // pri
      1, 156789, 7200,    // timestamp (sec/usec/zone)
      "",                // host
      "ctld",    // openvpn
      "snmpd[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7> Aug 29 02:00:00. ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
      7,             // pri
      1, 0, 7200,            // timestamp (sec/usec/zone)
      "",                // host
      "ctld",    // openvpn
      "snmpd[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7> Aug 29 02:00:00 ctld snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, "^ctld",
      7,             // pri
      1, 0, 7200,            // timestamp (sec/usec/zone)
      "",                // host
      "ctld",    // openvpn
      "snmpd[2499]: PTHREAD support initialized", // msg
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]: PTHREAD support initialized", LP_EXPECT_HOSTNAME, NULL,
      7,             // pri
      1, 0, 7200,            // timestamp (sec/usec/zone)
      "bzorp",            // host
      "ctld/snmpd",    // openvpn
      "PTHREAD support initialized", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {
      "<190>Apr 15 2007 21:28:13: %PIX-6-302014: Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs", LP_EXPECT_HOSTNAME, "^%",
      190,
      1176665293, 0, 7200,
      "",
      "%PIX-6-302014",
      "Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs",
      NULL, NULL, NULL, ignore_sdata_pairs
    },

    /* Dell switch */
    {
      "<190>NOV 22 00:00:33 192.168.33.8-1 CMDLOGGER[165319912]: cmd_logger_api.c(83) 13518 %% CLI:192.168.32.100:root:User  logged in", LP_EXPECT_HOSTNAME, NULL,
      190,
      _get_epoch_with_bsd_year(10, 22, 0, 0, 33)
      , 0, 3600,
      "192.168.33.8-1",
      "CMDLOGGER",
      "cmd_logger_api.c(83) 13518 %% CLI:192.168.32.100:root:User  logged in",
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_0)
{
  struct sdata_pair expected_sd_pairs_test_0[] =
  {
    { ".SDATA.timeQuality.isSynced", "1"},
    { NULL, NULL },
  };

  struct msgparse_params params[] =
  {
    {
      "<190>.Apr 15 2007 21:28:13: %PIX-6-302014: Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs", LP_EXPECT_HOSTNAME, "^%",
      190,
      1176665293, 0, 7200,
      "",
      "%PIX-6-302014",
      "Teardown TCP connection 1688438 for bloomberg-net:1.2.3.4/8294 to inside:5.6.7.8/3639 duration 0:07:01 bytes 16975 TCP FINs",
      NULL, NULL, NULL, expected_sd_pairs_test_0
    },
    {
      "<190>Apr 15 2007 21:28:13 %ASA: this is a Cisco ASA timestamp", LP_EXPECT_HOSTNAME, "^%",
      190,
      1176665293, 0, 7200,
      "",
      "%ASA",
      "this is a Cisco ASA timestamp",
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<190>Apr 15 21:28:13 2007 linksys app: msg", LP_EXPECT_HOSTNAME, NULL,
      190,
      1176665293, 0, 7200,
      "linksys",
      "app",
      "msg",
      NULL, NULL, NULL, ignore_sdata_pairs
    },
    {
      "<38>Sep 22 10:11:56 Message forwarded from cdaix66: sshd[679960]: Accepted publickey for nagios from 1.9.1.1 port 42096 ssh2", LP_EXPECT_HOSTNAME, NULL,
      38,
      1, 0, 7200,
      "cdaix66",
      "sshd",
      "Accepted publickey for nagios from 1.9.1.1 port 42096 ssh2",
      NULL, NULL, NULL, NULL
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_1)
{
  struct sdata_pair expected_sd_pairs_test_1[] =
  {
    { ".SDATA.exampleSDID@0.iut", "3"},
    { ".SDATA.exampleSDID@0.eventSource", "Application"},
    { ".SDATA.exampleSDID@0.eventID", "1011"},
    { ".SDATA.examplePriority@0.class", "high"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...", LP_SYSLOG_PROTOCOL, NULL,
      7,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine.example.com",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
      "",//processid
      "ID47",//msgid
      expected_sd_pairs_test_1
    },
    {
      "<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      7,             // pri
      1162087199, 156000, 0,    // timestamp (sec/usec/zone)
      "mymachine.example.com",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
      "",//processid
      "ID47",//msgid
      expected_sd_pairs_test_1
    },
    {
      "<7>1 2006-10-29T01:59:59.156123Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      7,             // pri
      1162087199, 156123, 0,    // timestamp (sec/usec/zone)
      "mymachine.example.com",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]", //sd_str
      "",//processid
      "ID47",//msgid
      expected_sd_pairs_test_1
    },
    {
      "<7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 [ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,             // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "",        // host
      "syslog-ng", //app
      "Error processing log message: <7>1 2006-10-29T01:59:59.156Z mymachine.example.com evntslog - ID47 >@<[ exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \xEF\xBB\xBF" "An application event log entry...", // msg
      "",
      NULL,//processid
      NULL,//msgid
      empty_sdata_pairs
    },
    {
      "<34>1 1987-01-01T12:00:27.000087+00:20 192.0.2.1 myproc 8710 - - %% It's time to make the do-nuts.", LP_SYSLOG_PROTOCOL, NULL,
      34,
      536499627, 87, 1200,
      "192.0.2.1",
      "myproc",
      "%% It's time to make the do-nuts.",
      "",
      "8710",
      "",
      ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_2)
{
  struct sdata_pair expected_sd_pairs_test_2[] =
  {
    { ".SDATA.exampleSDID@0.iut", "3"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\"] [eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,       // pri
      1162083599, 156000, 3600,  // timestamp (sec/usec/zone)
      "mymachine",   // host
      "evntslog", //app
      "[eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] An application event log entry...", // msg
      "[exampleSDID@0 iut=\"3\"]", //sd_str
      "",//processid
      "",//msgid
      expected_sd_pairs_test_2
    },
    {
      "<7>Aug 29 02:00:00 bzorp ctld/snmpd[2499]:", LP_EXPECT_HOSTNAME, NULL,
      7,           // pri
      1, 0, 7200,          // timestamp (sec/usec/zone)
      "bzorp",         // host
      "ctld/snmpd",    // openvpn
      "", // msg
      NULL, "2499", NULL, ignore_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_3)
{
  struct sdata_pair expected_sd_pairs_test_3[] =
  {
    { ".SDATA.origin.ip", "exchange.macartney.esbjerg"},
    { ".SDATA.meta.sequenceId", "191732"},
    { ".SDATA.EventData@18372.4.Data", "MSEXCHANGEOWAAPPPOOL.CONFIG\" -W \"\" -M 1 -AP \"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 " },
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"] ApplicationMSExchangeADAccess: message",
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
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_4)
{
  struct sdata_pair expected_sd_pairs_test_4[] =
  {
    { ".SDATA.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.i", "ok_32"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"ok_32\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"ok_32\"]", //sd_str
      "",//processid
      "",//msgid
      expected_sd_pairs_test_4
    },
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa i=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,                         // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "",         // host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa>@<aaa i=\"long_33\"] An application event log entry...", // msg
      "", //sd_str
      0,//processid
      0,//msgid
      empty_sdata_pairs
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_too_long)
{
  struct sdata_pair expected_sd_pairs_test_5[] =
  {
    { ".SDATA.a.i", "]\"\\"},
    {  NULL , NULL}
  };

  struct sdata_pair expected_sd_pairs_test_5b[] =
  {
    { ".SDATA.a.i", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\\]\\\"\\\\\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[a i=\"\\]\\\"\\\\\"]", //sd_str
      "",//processid
      "",//msgid
      expected_sd_pairs_test_5
    },

    // failed to parse to long sd name
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=\"long_33\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,             // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "",        // host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa>@<aaa=\"long_33\"] An application event log entry...", // msg
      "", //sd_str
      0,//processid
      0,//msgid
      empty_sdata_pairs
    },

    // too long sdata value gets truncated
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[a i=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]", //sd_str
      "",//processid
      "",//msgid
      expected_sd_pairs_test_5b
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_unescaped_too_long_message_parts)
{
  struct sdata_pair expected_sd_pairs_test_6[] =
  {
    { ".SDATA.a.i", "ok"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    // too long hostname
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa evntslog - - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,        // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "", //host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 >@<aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa evntslog - - [a i=\"ok\"] An application event log entry...",        // msg
      "", //sd_str
      0,//processid
      0,//msgid
      empty_sdata_pairs
    },

    // failed to parse to long appname
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", //app
      "An application event log entry...", // msg
      "[a i=\"ok\"]", //sd_str
      0,//processid
      0,//msgid
      expected_sd_pairs_test_6
    },

    // failed to parse to long procid
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa - [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,             // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[a i=\"ok\"]", //sd_str
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//processid
      0,//msgid
      expected_sd_pairs_test_6
    },

    // failed to parse to long msgid
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa [a i=\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      132,     // pri
      1162083599, 156000, 3600,    // timestamp (sec/usec/zone)
      "mymachine",        // host
      "evntslog", //app
      "An application event log entry...", // msg
      "[a i=\"ok\"]", //sd_str
      0,//processid
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",//msgid
      expected_sd_pairs_test_6
    },

    // unescaped ]
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"]ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,             // pri
      0, 0, 0,    // timestamp (sec/usec/zone)
      "",        // host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\">@<]ok\"] An application event log entry...", // msg
      "", //sd_str
      0,//processid
      0,//msgid
      empty_sdata_pairs
    },
    // unescaped '\'

    // bad sd data unescaped "
    {
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\"\"ok\"] An application event log entry...",  LP_SYSLOG_PROTOCOL, NULL,
      43,      // pri
      0, 0, 0, // timestamp (sec/usec/zone)
      "",    // host
      "syslog-ng", //app
      "Error processing log message: <132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [a i=\">@<\"ok\"] An application event log entry...", // msg
      "", //sd_str
      0,//processid
      0,//msgid
      0
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_tz_known)
{
  struct sdata_pair expected_sd_pairs_test_7a[] =
  {
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    //Testing syslog protocol message parsing if tzKnown=0 because there is no timezone information
    {
      "<134>1 2009-10-16T11:51:56 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - - An application event log entry...",
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
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_enterprise_id)
{
  struct sdata_pair expected_sd_pairs_test_8[] =
  {
    { ".SDATA.origin.enterpriseId", "1.3.6.1.4.1"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    {
      "<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin enterpriseId=\"1.3.6.1.4.1\"] An application event log entry...",
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
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_expected_sd_pairs_without_sd_param)
{
  struct sdata_pair expected_sd_pairs_test_9[] =
  {
    { ".SDATA.origin.enterpriseId", "1.3.6.1.4.1"},
    {  NULL , NULL}
  };

  struct msgparse_params params[] =
  {
    //Testing syslog protocol message parsing if SDATA contains only SD-ID without SD-PARAM
    //KNOWN BUG: 20459
    {
      "<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 - [origin enterpriseId=\"1.3.6.1.4.1\"][nosdnvpair] An application event log entry...",
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
    },
    {NULL}
  };

  run_parameterized_test(params);
}

Test(msgparse, test_ip_in_host)
{
  struct msgparse_params params[] =
  {
    (struct msgparse_params)
    {
      .msg = "<0>Jan 10 01:00:00 1.2.3.4 prg0",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_host = "1.2.3.4"
    },
    (struct msgparse_params)
    {
      .msg = "<0>Jan 10 01:00:00 0000:BABA:BA00:DAB:BABA:BABA:BABA:BAB0 prg0",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_host = "0000:BABA:BA00:DAB:BABA:BABA:BABA:BAB0"
    },
    (struct msgparse_params)
    {
      .msg = "<0>Jan 10 01:00:00 0001:BABA:BA00:DAB::BAB0 prg0",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_host = "0001:BABA:BA00:DAB::BAB0"
    },
    {
      .msg = "<0>Jan 10 01:00:00 0002:: prg0: msgtxt",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_host = "0002::",
      .expected_msg = "msgtxt"
    },
    (struct msgparse_params)
    {
      .msg = "<0>Jan 10 01:00:00 prg0", // No ip no msg
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_host = ""
    },
    (struct msgparse_params)
    {
      .msg = "<0>Jan 10 01:00:00 prg0: msgtxt", // program name with message, no ip
      .expected_stamp_sec =  _get_epoch_with_bsd_year(0, 10, 1, 0, 0),
      .expected_stamp_ofs = 3600,
      .expected_program = "prg0",
      .expected_msg = "msgtxt"
    },
    (struct msgparse_params)
    {
      .msg = "<0>91: *Oct 07 03:10:04: mydevice.com %CRYPTO-4-RECVD_PKT_INV_SPI: decaps: rec'd IPSEC packet has invalid spi for destaddr=150.1.1.1, prot=50, spi=0x72662541(1919296833), srcaddr=150.3.1.3",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .expected_stamp_sec = _get_epoch_with_bsd_year(9, 7, 3, 10, 4),
      .expected_stamp_ofs = 7200,
      .expected_program = "%CRYPTO-4-RECVD_PKT_INV_SPI",
      .expected_host = "mydevice.com",
      .expected_msg =
      "decaps: rec'd IPSEC packet has invalid spi for destaddr=150.1.1.1, prot=50, spi=0x72662541(1919296833), srcaddr=150.3.1.3"
    },
    {NULL}
  };
  run_parameterized_test(params);
}

Test(msgparse, test_simple_message)
{
  struct msgparse_params params[] =
  {
    {
      "some message",
      LP_EXPECT_HOSTNAME, NULL,
      13,
      0, 0, 0,
      "",
      "some",
      "message",
      "",
      0,
      0,
      empty_sdata_pairs
    },
    {NULL}
  };
  run_parameterized_test(params);
}
