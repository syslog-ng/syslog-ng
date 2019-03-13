/*
 * Copyright (c) 2009-2016 Balabit
 * Copyright (c) 2009-2014 Viktor Juhasz
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

#include "syslog-ng.h"
#include "logwriter.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "logqueue-fifo.h"
#include "timeutils/misc.h"

#include <criterion/criterion.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

MsgFormatOptions parse_options;

const gchar *MSG_SYSLOG_STR =
  "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" "
  "eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...";

const gchar *EXPECTED_MSG_SYSLOG_STR =
  "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" "
  "eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...\n";

const gchar *EXPECTED_MSG_SYSLOG_STR_T =
  "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" "
  "eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47 BOMAn application event log entry...\n";

const gchar *EXPECTED_MSG_SYSLOG_TO_BSD_STR =
  "<132>Oct 29 01:59:59 mymachine evntslog[3535]: BOMAn application event log entry...\n";

const gchar *EXPECTED_MSG_SYSLOG_TO_FILE_STR =
  "Oct 29 01:59:59 mymachine evntslog[3535]: BOMAn application event log entry...\n";

const gchar *MSG_SYSLOG_EMPTY_STR =
  "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 "
  "iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"]";

const gchar *EXPECTED_MSG_SYSLOG_EMPTY_STR =
  "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 "
  "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] \n";

const gchar *EXPECTED_MSG_SYSLOG_EMPTY_STR_T =
  "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47 "
  "[exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] ID47\n";

const gchar *MSG_BSD_STR = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

const gchar *EXPECTED_MSG_BSD_STR = "Feb 11 10:34:56 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";

const gchar *EXPECTED_MSG_BSD_STR_T = "155 23323 árvíztűrőtükörfúrógép";

const gchar *EXPECTED_MSG_BSD_TO_SYSLOG_STR =
  "<155>1 2006-02-11T10:34:56+01:00 bzorp syslog-ng 23323 - - árvíztűrőtükörfúrógép\n";

const gchar *EXPECTED_MSG_BSD_TO_PROTO_STR =
  "<155>Feb 11 10:34:56 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";

const gchar *MSG_ZERO_PRI = "<0>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

const gchar *EXPECTED_MSG_ZERO_PRI_STR = "<0>Feb 11 10:34:56 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";

const gchar *EXPECTED_MSG_ZERO_PRI_STR_T = "0";

const gchar *MSG_BSD_EMPTY_STR = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:";

const gchar *EXPECTED_MSG_BSD_EMPTY_STR = "<155>Feb 11 10:34:56 bzorp syslog-ng[23323]:\n";

const gchar *EXPECTED_MSG_BSD_EMPTY_STR_T = "23323";

typedef struct _LogWriterTestCase
{
  const gchar *msg;
  const gboolean is_rfc5424;
  const gchar *template;
  const guint writer_flags;
  const gchar *expected_value;
} LogWriterTestCase;

LogMessage *
init_msg(const gchar *msg_string, gboolean use_syslog_protocol)
{
  LogMessage *msg;
  GSockAddr *sa;

  if (use_syslog_protocol)
    parse_options.flags |= LP_SYSLOG_PROTOCOL;
  else
    parse_options.flags &= ~LP_SYSLOG_PROTOCOL;
  sa = g_sockaddr_inet_new("10.10.10.10", 1010);
  msg = log_msg_new(msg_string, strlen(msg_string), sa, &parse_options);
  g_sockaddr_unref(sa);
  log_msg_set_value_by_name(msg, "APP.VALUE", "value", 5);
  log_msg_set_match(msg, 0, "whole-match", 11);
  log_msg_set_match(msg, 1, "first-match", 11);

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", 9);
  msg->timestamps[LM_TS_RECVD].ut_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].ut_usec = 639000;
  msg->timestamps[LM_TS_RECVD].ut_gmtoff = get_local_timezone_ofs(1139684315);
  return msg;
}

void
_tear_down(LogTemplate *template, LogWriter *writer, LogMessage *msg, LogQueue *queue, GString *result_msg)
{
  if (template)
    log_template_unref(template);
  log_pipe_unref((LogPipe *) writer);
  log_msg_unref(msg);
  log_queue_unref(queue);
  g_string_free(result_msg, TRUE);
}

void
_assert_logwriter_output(LogWriterTestCase c)
{
  LogTemplate *templ = NULL;
  LogWriter *writer = NULL;
  GString *result_msg = g_string_sized_new(128);
  GError *error = NULL;
  LogMessage *msg = NULL;
  LogWriterOptions opt = {0};
  LogQueue *queue;

  static TimeZoneInfo *tzinfo = NULL;

  if (!tzinfo)
    tzinfo = time_zone_info_new(NULL);
  opt.options = LWO_NO_MULTI_LINE | LWO_NO_STATS | LWO_SHARE_STATS;
  opt.template_options.time_zone_info[LTZ_SEND]=tzinfo;

  if (c.template)
    {
      templ = log_template_new(configuration, "dummy");
      log_template_compile(templ, c.template, &error);
    }
  opt.template = templ;
  msg = init_msg(c.msg, c.is_rfc5424);
  queue = log_queue_fifo_new(1000, NULL);
  writer = log_writer_new(c.writer_flags, configuration);

  log_writer_set_options(writer, NULL, &opt, NULL, NULL);
  log_writer_set_queue(writer, queue);
  log_writer_format_log(writer, msg, result_msg);
  cr_assert_str_eq(result_msg->str, c.expected_value, "Expected: %s, actual: %s", c.expected_value, result_msg->str);

  _tear_down(templ, writer, msg, queue, result_msg);
}

Test(logwriter, test_logwriter)
{
  configuration = cfg_new_snippet();
  LogWriterTestCase test_cases[] =
  {
    {MSG_SYSLOG_STR, TRUE, NULL, LW_SYSLOG_PROTOCOL, EXPECTED_MSG_SYSLOG_STR},
    {MSG_SYSLOG_STR, TRUE, "$MSGID $MSG", LW_SYSLOG_PROTOCOL, EXPECTED_MSG_SYSLOG_STR_T},
    {MSG_SYSLOG_EMPTY_STR, TRUE, NULL, LW_SYSLOG_PROTOCOL, EXPECTED_MSG_SYSLOG_EMPTY_STR},
    {MSG_SYSLOG_EMPTY_STR, TRUE, "$MSGID", LW_SYSLOG_PROTOCOL, EXPECTED_MSG_SYSLOG_EMPTY_STR_T},
    {MSG_SYSLOG_STR, TRUE, NULL, LW_FORMAT_PROTO, EXPECTED_MSG_SYSLOG_TO_BSD_STR},
    {MSG_SYSLOG_STR, TRUE, NULL, LW_FORMAT_FILE, EXPECTED_MSG_SYSLOG_TO_FILE_STR},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_FILE, EXPECTED_MSG_BSD_STR},
    {MSG_BSD_STR, FALSE, "$PRI $PID $MSG", LW_FORMAT_FILE, EXPECTED_MSG_BSD_STR_T},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_PROTO, EXPECTED_MSG_BSD_TO_PROTO_STR},
    {MSG_BSD_STR, FALSE, NULL, LW_SYSLOG_PROTOCOL, EXPECTED_MSG_BSD_TO_SYSLOG_STR},
    {MSG_BSD_EMPTY_STR, FALSE, NULL, LW_FORMAT_PROTO, EXPECTED_MSG_BSD_EMPTY_STR},
    {MSG_BSD_EMPTY_STR, FALSE, "$PID", LW_FORMAT_PROTO, EXPECTED_MSG_BSD_EMPTY_STR_T},
    {MSG_ZERO_PRI, FALSE, NULL, LW_FORMAT_PROTO, EXPECTED_MSG_ZERO_PRI_STR},
    {MSG_ZERO_PRI, FALSE, "$PRI", LW_FORMAT_PROTO, EXPECTED_MSG_ZERO_PRI_STR_T},
  };
  gint i, nr_of_cases;

  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  nr_of_cases = sizeof(test_cases) / sizeof(test_cases[0]);
  for (i = 0; i < nr_of_cases; i++)
    _assert_logwriter_output(test_cases[i]);

  app_shutdown();
}
