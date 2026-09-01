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

#include <criterion/criterion.h>

#define LOGWRITER_TEST_PRIVATE

#include "logwriter.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "logqueue-fifo.h"
#include "timeutils/cache.h"
#include "msg-format.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iv.h>

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

const gchar *EXPECTED_MSG_SYSLOG_STR_T_TRUNCATE =
  "<132>1 2006-10-29T01:59:59+01:00 mymachine evntslog 3535 ID47";

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

const gchar *EXPECTED_MSG_BSD_STR_TRUNCATE = "Feb 11 10:34:56 bzorp syslog-ng[23323]:";

const gchar *EXPECTED_MSG_BSD_STR_T = "155 23323 árvíztűrőtükörfúrógép";

const gchar *EXPECTED_MSG_BSD_STR_T_TRUNCATE = "155 23323 árvíztűrő";

const gchar *EXPECTED_MSG_BSD_TO_SYSLOG_STR =
  "<155>1 2006-02-11T10:34:56+01:00 bzorp syslog-ng 23323 - - árvíztűrőtükörfúrógép\n";

const gchar *EXPECTED_MSG_BSD_TO_PROTO_STR =
  "<155>Feb 11 10:34:56 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép\n";

const gchar *EXPECTED_MSG_BSD_TO_PROTO_STR_TRUNCATE = "<155>Feb 11 10:34:56 bz";

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
  gint truncate_size;
  const gchar *expected_value;
} LogWriterTestCase;

LogMessage *
init_msg(const gchar *msg_string, gboolean use_syslog_protocol)
{
  LogMessage *msg;

  if (use_syslog_protocol)
    parse_options.flags |= LP_SYSLOG_PROTOCOL;
  else
    parse_options.flags &= ~LP_SYSLOG_PROTOCOL;
  msg = msg_format_parse(&parse_options, (const guchar *) msg_string, strlen(msg_string));
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
_tear_down(LogWriter *writer, LogMessage *msg, LogQueue *queue, GString *result_msg, LogWriterOptions *writer_options)
{
  cr_expect(log_pipe_deinit((LogPipe *)writer));
  log_pipe_unref((LogPipe *) writer);
  log_msg_unref(msg);
  log_queue_unref(queue);
  g_string_free(result_msg, TRUE);
  log_writer_options_destroy(writer_options);
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

  TimeZoneInfo *tzinfo = time_zone_info_new(NULL);

  log_writer_options_defaults(&opt);
  opt.template_options.time_zone_info[LTZ_SEND] = tzinfo;
  log_writer_options_init(&opt, configuration, LWO_NO_MULTI_LINE | LWO_NO_STATS);
  if (c.truncate_size > 0)
    opt.truncate_size = c.truncate_size;

  if (c.template)
    {
      templ = log_template_new(configuration, "dummy");
      cr_assert(log_template_compile(templ, c.template, &error));
      opt.template = templ;
    }
  msg = init_msg(c.msg, c.is_rfc5424);
  queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  writer = log_writer_new(c.writer_flags, configuration);

  log_writer_set_options(writer, NULL, &opt, NULL, NULL);
  log_writer_set_queue(writer, queue);
  cr_assert(log_pipe_init((LogPipe *)writer), "LogWriter initialization failed");
  log_writer_format_log(writer, msg, result_msg);
  cr_assert_str_eq(result_msg->str, c.expected_value, "Expected: %s, actual: %s (truncate_size:%d)",
                   c.expected_value, result_msg->str, c.truncate_size);

  _tear_down(writer, msg, queue, result_msg, &opt);
}

Test(logwriter, test_logwriter)
{
  configuration = cfg_new_snippet();
  LogWriterTestCase test_cases[] =
  {
    {MSG_SYSLOG_STR, TRUE, NULL, LW_SYSLOG_PROTOCOL, 0, EXPECTED_MSG_SYSLOG_STR},
    {MSG_SYSLOG_STR, TRUE, "$MSGID $MSG", LW_SYSLOG_PROTOCOL, 0, EXPECTED_MSG_SYSLOG_STR_T},
    {MSG_SYSLOG_STR, TRUE, "$MSGID $MSG", LW_SYSLOG_PROTOCOL, strlen(EXPECTED_MSG_SYSLOG_STR_T_TRUNCATE), EXPECTED_MSG_SYSLOG_STR_T_TRUNCATE},
    // test that truncate doesn't apply on smaller messages
    {MSG_SYSLOG_STR, TRUE, "$MSGID $MSG", LW_SYSLOG_PROTOCOL, strlen(EXPECTED_MSG_SYSLOG_STR_T), EXPECTED_MSG_SYSLOG_STR_T},
    {MSG_SYSLOG_EMPTY_STR, TRUE, NULL, LW_SYSLOG_PROTOCOL, 0, EXPECTED_MSG_SYSLOG_EMPTY_STR},
    {MSG_SYSLOG_EMPTY_STR, TRUE, "$MSGID", LW_SYSLOG_PROTOCOL, 0, EXPECTED_MSG_SYSLOG_EMPTY_STR_T},
    {MSG_SYSLOG_STR, TRUE, NULL, LW_FORMAT_PROTO, 0, EXPECTED_MSG_SYSLOG_TO_BSD_STR},
    {MSG_SYSLOG_STR, TRUE, NULL, LW_FORMAT_FILE, 0, EXPECTED_MSG_SYSLOG_TO_FILE_STR},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_FILE, 0, EXPECTED_MSG_BSD_STR},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_FILE, strlen(EXPECTED_MSG_BSD_STR_TRUNCATE), EXPECTED_MSG_BSD_STR_TRUNCATE},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_FILE, strlen(EXPECTED_MSG_BSD_STR), EXPECTED_MSG_BSD_STR},
    {MSG_BSD_STR, FALSE, "$PRI $PID $MSG", LW_FORMAT_FILE, 0, EXPECTED_MSG_BSD_STR_T},
    {MSG_BSD_STR, FALSE, "$PRI $PID $MSG", LW_FORMAT_FILE, strlen(EXPECTED_MSG_BSD_STR_T_TRUNCATE), EXPECTED_MSG_BSD_STR_T_TRUNCATE},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_PROTO, 0, EXPECTED_MSG_BSD_TO_PROTO_STR},
    {MSG_BSD_STR, FALSE, NULL, LW_FORMAT_PROTO, strlen(EXPECTED_MSG_BSD_TO_PROTO_STR_TRUNCATE), EXPECTED_MSG_BSD_TO_PROTO_STR_TRUNCATE},
    {MSG_BSD_STR, FALSE, NULL, LW_SYSLOG_PROTOCOL, 0, EXPECTED_MSG_BSD_TO_SYSLOG_STR},
    {MSG_BSD_EMPTY_STR, FALSE, NULL, LW_FORMAT_PROTO, 0, EXPECTED_MSG_BSD_EMPTY_STR},
    {MSG_BSD_EMPTY_STR, FALSE, "$PID", LW_FORMAT_PROTO, 0, EXPECTED_MSG_BSD_EMPTY_STR_T},
    {MSG_ZERO_PRI, FALSE, NULL, LW_FORMAT_PROTO, 0, EXPECTED_MSG_ZERO_PRI_STR},
    {MSG_ZERO_PRI, FALSE, "$PRI", LW_FORMAT_PROTO, 0, EXPECTED_MSG_ZERO_PRI_STR_T},
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

  msg_format_options_destroy(&parse_options);
  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}

/* Regression test for a reconnect hang: log_writer_reopen_deferred() can
 * defer applying a NULL LogProtoClient while the writer's I/O job is still
 * in flight (e.g. a connection break detected concurrently with an
 * in-progress write). When that job later completes,
 * log_writer_work_finished() must not silently leave the writer with no
 * proto and no watches running -- it has to request a reconnect,
 * otherwise nothing would ever wake the writer up again. */

typedef struct _MockControlPipe
{
  LogPipe super;
  gint notify_count;
  gint last_notify_code;
} MockControlPipe;

static gint
_mock_control_pipe_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  MockControlPipe *self = (MockControlPipe *) s;

  self->notify_count++;
  self->last_notify_code = notify_code;
  return NR_OK;
}

static MockControlPipe *
_mock_control_pipe_new(GlobalConfig *cfg)
{
  MockControlPipe *self = g_new0(MockControlPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.notify = _mock_control_pipe_notify;
  self->last_notify_code = -1;
  return self;
}

Test(logwriter, test_logwriter_requests_reconnect_after_reopen_deferred_during_active_io_job)
{
  configuration = cfg_new_snippet();
  app_startup();

  LogWriterOptions opt = {0};
  log_writer_options_defaults(&opt);
  log_writer_options_init(&opt, configuration, LWO_NO_STATS);

  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  LogWriter *writer = log_writer_new(0, configuration);
  MockControlPipe *control = _mock_control_pipe_new(configuration);

  log_writer_set_options(writer, &control->super, &opt, NULL, NULL);
  log_writer_set_queue(writer, queue);
  cr_assert(log_pipe_init((LogPipe *) writer), "LogWriter initialization failed");

  /* A connection break arrives while the writer's I/O job is still
   * considered in flight -- log_writer_reopen_deferred() must take the
   * deferred branch instead of applying the NULL proto immediately. */
  log_writer_test_set_io_job_working(writer, TRUE);
  log_writer_test_call_reopen_deferred(writer, NULL);
  cr_assert_not(log_writer_test_get_watches_running(writer),
                "watches must not be running while a reopen is still deferred");

  /* The in-flight job now completes. This applies the deferred NULL proto
   * and used to leave the writer stuck forever with no proto and no
   * watches running -- it must request a reconnect instead. */
  log_writer_test_simulate_io_job_completion(writer, TRUE);

  cr_assert_eq(control->notify_count, 1,
               "expected exactly one notification requesting a reconnect after the deferred NULL proto was applied");
  cr_assert_eq(control->last_notify_code, NC_REOPEN_REQUIRED,
               "log_writer_work_finished() must request a reconnect (NC_REOPEN_REQUIRED) after applying a deferred "
               "NULL proto, otherwise the writer is left stuck with no fd watch and no timer to wake it up again");

  cr_expect(log_pipe_deinit((LogPipe *) writer));
  log_pipe_unref((LogPipe *) writer);
  log_pipe_unref(&control->super);
  log_queue_unref(queue);
  log_writer_options_destroy(&opt);

  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}
