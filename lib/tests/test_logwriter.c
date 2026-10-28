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

#include "logwriter.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"
#include "logqueue-fifo.h"
#include "timeutils/cache.h"
#include "msg-format.h"
#include "libtest/mock-transport.h"
#include "logproto/logproto-text-client.h"
#include "mainloop-call.h"
#include "mainloop-io-worker.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iv.h>
#include <iv_event.h>
#include <unistd.h>

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

/* Regression tests: a valid proto must cancel any pending reopen_timer,
 * otherwise it can fire later and trigger a spurious duplicate
 * reconnect. */

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

/* Real LogProtoClient over a LogTransportMock (as in test_file_writer.c).
 * The pipe's read end is used so the fd never becomes readable/writable,
 * keeping LogWriter's real fd-watch idle for the test's lifetime. */
static LogProtoClient *
_new_mock_proto(LogProtoClientOptionsStorage *proto_options, gint *fds_out)
{
  gint fds[2];

  cr_assert_eq(pipe(fds), 0, "failed to create pipe for mock transport");

  LogTransport *transport = log_transport_mock_stream_new(LTM_EOF);
  transport->fd = fds[0];

  fds_out[0] = fds[0];
  fds_out[1] = fds[1];

  return log_proto_text_client_new(transport, proto_options);
}

static LogWriter *
_construct_writer(GlobalConfig *cfg, LogWriterOptions *opt, MockControlPipe *control, LogQueue *queue)
{
  LogWriter *writer = log_writer_new(0, cfg);

  log_writer_set_options(writer, &control->super, opt, NULL, NULL);
  log_writer_set_queue(writer, queue);
  cr_assert(log_pipe_init((LogPipe *) writer), "LogWriter initialization failed");
  return writer;
}

static void
_stop_main_loop(gpointer user_data)
{
  iv_quit();
}

/* Runs the real ivykis loop for a bounded time, letting any registered
 * timer (e.g. reopen_timer) actually fire. */
static void
_run_main_loop_for(gint seconds)
{
  struct iv_timer stop_timer;

  iv_validate_now();
  IV_TIMER_INIT(&stop_timer);
  stop_timer.cookie = NULL;
  stop_timer.handler = _stop_main_loop;
  stop_timer.expires = iv_now;
  stop_timer.expires.tv_sec += seconds;

  iv_timer_register(&stop_timer);
  iv_main();
  if (iv_timer_registered(&stop_timer))
    iv_timer_unregister(&stop_timer);
}

Test(logwriter,
     test_logwriter_installing_a_valid_proto_cancels_a_pending_reopen_timer)
{
  configuration = cfg_new_snippet();
  app_startup();

  LogWriterOptions opt = {0};
  log_writer_options_defaults(&opt);
  log_writer_options_init(&opt, configuration, LWO_NO_STATS);
  opt.time_reopen = 1;

  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  MockControlPipe *control = _mock_control_pipe_new(configuration);
  LogWriter *writer = _construct_writer(configuration, &opt, control, queue);

  /* A NULL reopen on this already-closed writer arms its retry timer. */
  log_writer_reopen(writer, NULL);
  cr_assert_not(log_writer_opened(writer));
  cr_assert_eq(control->notify_count, 0,
               "installing a NULL proto must not notify by itself");

  /* Act as the owner and provide a valid proto before the timer fires. */
  gint fds[2];
  LogProtoClient *proto = _new_mock_proto(&opt.proto_options, fds);
  log_writer_reopen(writer, proto);
  cr_assert(log_writer_opened(writer));

  /* Without cancellation, the stale timer would request a reconnect even
   * though the writer already has a valid proto. No earlier notification is expected. */
  _run_main_loop_for(opt.time_reopen + 1);

  cr_assert_eq(control->notify_count, 0,
               "a stale reopen_timer fired and requested a duplicate reconnect "
               "even though a valid proto was already installed");

  cr_expect(log_pipe_deinit((LogPipe *) writer));
  log_pipe_unref((LogPipe *) writer);
  log_pipe_unref(&control->super);
  log_queue_unref(queue);
  log_writer_options_destroy(&opt);
  close(fds[0]);
  close(fds[1]);

  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}

Test(logwriter,
     test_logwriter_repeated_failed_reopen_leaves_exactly_one_pending_timer)
{
  configuration = cfg_new_snippet();
  app_startup();

  LogWriterOptions opt = {0};
  log_writer_options_defaults(&opt);
  log_writer_options_init(&opt, configuration, LWO_NO_STATS);
  opt.time_reopen = 1;

  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  MockControlPipe *control = _mock_control_pipe_new(configuration);
  LogWriter *writer = _construct_writer(configuration, &opt, control, queue);

  /* Two NULL reopen requests must leave only one pending retry timer;
   * this test does not perform actual destination open attempts. */
  log_writer_reopen(writer, NULL);
  log_writer_reopen(writer, NULL);
  cr_assert_not(log_writer_opened(writer));
  cr_assert_eq(control->notify_count, 0);

  _run_main_loop_for(opt.time_reopen + 1);

  cr_assert_eq(control->notify_count, 1,
               "expected exactly one reconnect request from the pending reopen timer");
  cr_assert_eq(control->last_notify_code, NC_REOPEN_REQUIRED);

  cr_expect(log_pipe_deinit((LogPipe *) writer));
  log_pipe_unref((LogPipe *) writer);
  log_pipe_unref(&control->super);
  log_queue_unref(queue);
  log_writer_options_destroy(&opt);

  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}

/* These cases use real writer I/O dispatch. Threaded jobs pause in post() or
 * process_in(); the main thread submits requests selected by the test mode,
 * including modes with no reopen. Synchronous cases do not pause and observe
 * completion through a subsequent main-loop task. A second event-loop window
 * checks delayed retries or stale notifications. Transport and owner responses
 * are controlled doubles, not actual destination connections. */
typedef enum
{
  REOPEN_NONE,
  REOPEN_NULL,
  REOPEN_NULL_REPEATED,
  REOPEN_VALID_PENDING,
  REOPEN_VALID_ONLY,
  REOPEN_TIMER_RECOVERY,
  REOPEN_OWNER_RECOVERY,
  REOPEN_WORKER_ROTATION,
  REOPEN_WORKER_ROTATION_ONLY,
} ThreadedReopenMode;

typedef struct
{
  MockControlPipe control;
  LogWriter *writer;
  LogWriterOptions options;
  ThreadedReopenMode mode;
  gboolean threaded_io;
  LogProtoStatus work_result;
  gint notify_codes[NC_LOGROTATE + 1];
  LogProtoClient *initial_proto;
  LogProtoClient *replacement_proto;
  LogProtoClient *installed_rotation_proto;
  GMutex lock;
  GCond cond;
  gboolean released;
  gboolean completed;
  gboolean rotated_in_worker;
  struct iv_event blocked;
  struct iv_timer release_timer;
  struct iv_task synchronous_completion;
  LogProtoClient *rotation_proto;
  LogProtoStatus (*original_post)(LogProtoClient *, LogMessage *, guchar *, gsize, gboolean *);
  LogProtoStatus (*original_flush)(LogProtoClient *);
  gint replacement_fds[2];
  gint rotation_fds[2];
} ThreadedReopenCase;

static ThreadedReopenCase *threaded_reopen_case;
static void _worker_quiesced(gpointer user_data);

static gboolean
_expects_open_writer(ThreadedReopenMode mode)
{
  return mode == REOPEN_NONE || mode == REOPEN_VALID_PENDING || mode == REOPEN_VALID_ONLY
         || mode == REOPEN_TIMER_RECOVERY || mode == REOPEN_OWNER_RECOVERY || mode == REOPEN_WORKER_ROTATION_ONLY;
}

static LogProtoStatus
_completion_flush(LogProtoClient *proto)
{
  ThreadedReopenCase *test_case = threaded_reopen_case;

  /* Run the real flush, then substitute the requested completion status only
   * if it succeeded. Rotation's old-proto flush keeps its actual status so
   * the worker can install the replacement before the final flush is injected. */
  LogProtoStatus result = test_case->original_flush(proto);
  if ((test_case->mode == REOPEN_WORKER_ROTATION || test_case->mode == REOPEN_WORKER_ROTATION_ONLY)
      && proto == test_case->initial_proto)
    return result;
  return result == LPS_SUCCESS ? test_case->work_result : result;
}

static void
_wait_for_reopen_request(void)
{
  ThreadedReopenCase *test_case = threaded_reopen_case;

  if (!test_case->threaded_io)
    {
      /* A task runs after the synchronous I/O callback has returned through
       * work_finished(), so the observer still sees completed writer state. */
      g_assert(main_loop_is_main_thread());
      iv_task_register(&test_case->synchronous_completion);
      return;
    }
  g_assert(main_loop_worker_is_worker_thread());
  /* Notify the main loop, then keep post() or process_in() in flight until
   * the main-thread handler releases it after the selected request sequence. */
  iv_event_post(&test_case->blocked);
  g_mutex_lock(&test_case->lock);
  gint64 deadline = g_get_monotonic_time() + 10 * G_USEC_PER_SEC;
  while (!test_case->released)
    if (!g_cond_wait_until(&test_case->cond, &test_case->lock, deadline))
      g_error("Timed out waiting for the main-thread reopen request");
  g_mutex_unlock(&test_case->lock);
}

static LogProtoStatus
_blocked_process_in(LogProtoClient *proto)
{
  gchar marker;
  g_assert(read(log_proto_client_get_fd(proto), &marker, 1) == 1);
  _wait_for_reopen_request();
  return threaded_reopen_case->work_result;
}

static LogProtoStatus
_blocked_post(LogProtoClient *proto, LogMessage *msg, guchar *buffer, gsize length, gboolean *consumed)
{
  ThreadedReopenCase *test_case = threaded_reopen_case;

  _wait_for_reopen_request();

  /* Preserve the original post() ownership and consumed-message handling.
   * Output completion status is controlled separately by _completion_flush(). */
  return test_case->original_post(proto, msg, buffer, length, consumed);
}

static void
_worker_quiesced(gpointer user_data)
{
  ThreadedReopenCase *test_case = user_data;

  /* Worker synchronization, or a subsequent task in synchronous cases, invokes
   * this on main after work_finished() has handled any pending proto and notifications. */
  test_case->completed = TRUE;
  iv_quit();
}

static void
_release_blocked_worker(gpointer user_data)
{
  ThreadedReopenCase *test_case = user_data;

  /* Both delayed modes have passed the original retry deadline. A valid
   * request must have suppressed it; timer recovery must have emitted once.
   * Their replacement is still pending because I/O has not been released. */
  if (test_case->mode == REOPEN_VALID_PENDING)
    cr_assert_eq(test_case->control.notify_count, 0,
                 "a valid pending proto must cancel the retry before I/O completion");
  else if (test_case->mode == REOPEN_TIMER_RECOVERY)
    cr_assert_eq(test_case->control.notify_count, 1,
                 "the real retry timer must request recovery before I/O completion");

  /* Request quiescence before releasing the blocked I/O callback. This
   * prevents new jobs and observes completion through the worker-sync callback. */
  main_loop_worker_sync_call(_worker_quiesced, test_case);
  g_mutex_lock(&test_case->lock);
  test_case->released = TRUE;
  g_cond_signal(&test_case->cond);
  g_mutex_unlock(&test_case->lock);
}

static void
_request_valid_reopen(ThreadedReopenCase *test_case)
{
  test_case->replacement_proto = _new_mock_proto(&test_case->options.proto_options,
                                                 test_case->replacement_fds);
  log_writer_reopen(test_case->writer, test_case->replacement_proto);
}

static void
_request_reopen_while_worker_is_blocked(gpointer user_data)
{
  ThreadedReopenCase *test_case = user_data;

  /* The worker is blocked inside post() or process_in(); every reopen below
   * must remain deferred until that I/O callback is released. */
  switch (test_case->mode)
    {
    case REOPEN_NONE:
    case REOPEN_WORKER_ROTATION_ONLY:
      break;

    case REOPEN_NULL:
    case REOPEN_TIMER_RECOVERY:
    case REOPEN_OWNER_RECOVERY:
    case REOPEN_WORKER_ROTATION:
      log_writer_reopen(test_case->writer, NULL);
      break;

    case REOPEN_NULL_REPEATED:
      log_writer_reopen(test_case->writer, NULL);
      log_writer_reopen(test_case->writer, NULL);
      break;

    case REOPEN_VALID_PENDING:
      log_writer_reopen(test_case->writer, NULL);
      _request_valid_reopen(test_case);
      break;

    case REOPEN_VALID_ONLY:
      _request_valid_reopen(test_case);
      break;

    default:
      g_assert_not_reached();
    }

  cr_assert_eq(log_writer_get_proto(test_case->writer), test_case->initial_proto,
               "a deferred request replaced the proto still used by the worker");

  if (test_case->mode == REOPEN_VALID_PENDING || test_case->mode == REOPEN_TIMER_RECOVERY)
    {
      /* Keep I/O blocked beyond the old retry deadline. */
      iv_validate_now();
      test_case->release_timer.expires = iv_now;
      test_case->release_timer.expires.tv_sec += 2;
      iv_timer_register(&test_case->release_timer);
    }
  else
    _release_blocked_worker(test_case);
}

static gint
_threaded_control_notify(LogPipe *pipe, gint notify_code, gpointer user_data)
{
  ThreadedReopenCase *test_case = (ThreadedReopenCase *) pipe;

  if (notify_code == NC_LOGROTATE)
    {
      if (test_case->rotation_proto)
        {
          /* Return a replacement through the real rotation callback contract.
           * The writer, not this mock handler, installs it in the worker. */
          gpointer *args = user_data;
          LogProtoClient **replacement = args[0];
          *replacement = test_case->rotation_proto;
          test_case->rotation_proto = NULL;
          test_case->rotated_in_worker = main_loop_worker_is_worker_thread();
        }
      return NR_OK;
    }

  /* Completion and retry notifications must run on main in both execution
   * modes. Most cases only count them; recovery modes also respond through
   * the public API to exercise owner callbacks re-entering the writer. */
  cr_assert(main_loop_is_main_thread(), "reconnect notifications must run on the main thread");
  cr_assert_geq(notify_code, 0);
  cr_assert_leq(notify_code, NC_LOGROTATE);
  test_case->notify_codes[notify_code]++;
  if ((notify_code == NC_CLOSE || notify_code == NC_WRITE_ERROR) && test_case->mode == REOPEN_OWNER_RECOVERY)
    log_writer_reopen(test_case->writer, NULL);
  if (notify_code == NC_REOPEN_REQUIRED
      && (test_case->mode == REOPEN_TIMER_RECOVERY || test_case->mode == REOPEN_OWNER_RECOVERY)
      && !test_case->replacement_proto)
    {
      /* Timer-recovery mode responds while I/O is blocked, so installation
       * is deferred. Owner-recovery mode responds during completion or a
       * later retry, when the replacement can be installed immediately. */
      test_case->replacement_proto = _new_mock_proto(&test_case->options.proto_options,
                                                     test_case->replacement_fds);
      log_writer_reopen(test_case->writer, test_case->replacement_proto);
    }
  return _mock_control_pipe_notify(pipe, notify_code, user_data);
}

static void
_assert_writer_reopen_io(ThreadedReopenMode mode, LogProtoStatus work_result, gboolean input, gboolean threaded)
{
  /* Initialize the real worker pool and cross-thread main-loop dispatch.
   * The configuration hooks reserve and finalize worker thread resources. */
  configuration = cfg_new_snippet();
  app_startup();
  main_loop_worker_init();
  main_loop_io_worker_init();
  main_loop_call_init();
  app_config_pre_pre_init();
  app_config_pre_init();

  /* Protocol callbacks access this fixture through a global pointer. Static
   * storage prevents a stack address from escaping; reset it for each case.
   * Criterion isolates cases in separate processes, and teardown drains workers. */
  static ThreadedReopenCase test_case;
  test_case = (ThreadedReopenCase)
  {
    .mode = mode,
    .threaded_io = threaded,
    .work_result = work_result,
    .replacement_fds = { -1, -1 },
    .rotation_fds = { -1, -1 },
  };
  threaded_reopen_case = &test_case;
  g_mutex_init(&test_case.lock);
  g_cond_init(&test_case.cond);
  /* Count each notification kind separately. Only the explicit recovery
   * modes reconnect, so error/EOF cannot masquerade as a retry notification. */
  log_pipe_init_instance(&test_case.control.super, configuration);
  test_case.control.super.notify = _threaded_control_notify;
  log_writer_options_defaults(&test_case.options);
  log_writer_options_init(&test_case.options, configuration, LWO_NO_STATS | LWO_THREADED);
  if (!threaded)
    test_case.options.options &= ~LWO_THREADED;
  test_case.options.time_reopen = 1;

  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  test_case.writer = _construct_writer(configuration, &test_case.options, &test_case.control, queue);
  /* The worker posts this event; its handler submits reopen requests on main.
   * Pending-valid and timer-recovery cases delay release past the deadline. */
  IV_EVENT_INIT(&test_case.blocked);
  test_case.blocked.cookie = &test_case;
  test_case.blocked.handler = _request_reopen_while_worker_is_blocked;
  iv_event_register(&test_case.blocked);
  IV_TIMER_INIT(&test_case.release_timer);
  test_case.release_timer.cookie = &test_case;
  test_case.release_timer.handler = _release_blocked_worker;
  IV_TASK_INIT(&test_case.synchronous_completion);
  test_case.synchronous_completion.cookie = &test_case;
  test_case.synchronous_completion.handler = _worker_quiesced;

  /* A real ready fd drives normal poll/worker dispatch. For input, a byte
   * makes the read end ready; for output, the pipe's write end is ready. */
  gint active_fds[2];
  cr_assert_eq(pipe(active_fds), 0);
  LogTransport *transport = log_transport_mock_stream_new(LTM_EOF);
  transport->fd = active_fds[input ? 0 : 1];
  LogProtoClient *proto = input ? log_proto_text_client_new(transport, &test_case.options.proto_options)
                          : log_proto_unidirectional_text_client_new(transport, &test_case.options.proto_options);
  test_case.original_post = proto->post;
  proto->post = _blocked_post;
  test_case.initial_proto = proto;
  test_case.original_flush = proto->flush;
  proto->flush = _completion_flush;
  if (input)
    proto->process_in = _blocked_process_in;

  /* Prebuild the rotation replacement on the main thread. The mock control
   * hands it to log_writer_logrotate() after the blocked write resumes. */
  if (mode == REOPEN_WORKER_ROTATION || mode == REOPEN_WORKER_ROTATION_ONLY)
    {
      test_case.rotation_proto = _new_mock_proto(&test_case.options.proto_options, test_case.rotation_fds);
      test_case.installed_rotation_proto = test_case.rotation_proto;
      test_case.rotation_proto->flush = _completion_flush;
    }

  /* Trigger the selected I/O direction without calling private writer hooks. */
  if (input)
    cr_assert_eq(write(active_fds[1], "x", 1), 1);
  else
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      LogMessage *msg = log_msg_new_empty();
      log_msg_set_value(msg, LM_V_MESSAGE, "threaded reopen regression", -1);
      log_queue_push_tail(queue, msg, &path_options);
    }
  log_writer_reopen(test_case.writer, proto);
  /* Ten seconds is a failure bound, not a fixed wait: _worker_quiesced()
   * exits after completion, via worker sync or the synchronous observer task. */
  _run_main_loop_for(10);
  cr_assert(test_case.completed, "the blocked I/O job did not complete");

  gboolean failed = work_result != LPS_SUCCESS && work_result != LPS_PARTIAL;
  gboolean open_before_retry = _expects_open_writer(mode) && !(mode == REOPEN_OWNER_RECOVERY && failed);
  gint immediate_retries = mode == REOPEN_TIMER_RECOVERY
                           || ((!_expects_open_writer(mode) || mode == REOPEN_OWNER_RECOVERY) && !failed);
  /* Check before allowing another timer window: a later retry must not hide
   * a missing immediate notification. EOF/error notify with their own codes;
   * a later retry exists only if a NULL request or the mock owner armed one. */
  cr_assert_eq(test_case.notify_codes[NC_REOPEN_REQUIRED], immediate_retries,
               "completion did not deliver the expected immediate reconnect notification");
  cr_assert_eq(test_case.notify_codes[NC_CLOSE], work_result == LPS_EOF);
  cr_assert_eq(test_case.notify_codes[NC_WRITE_ERROR], failed && work_result != LPS_EOF);

  /* Validate proto identity, not only open/closed state. A deferred NULL must
   * still close a worker-installed rotation proto, while an accepted valid
   * replacement must win over the old proto even for EOF/error completion. */
  if (mode == REOPEN_WORKER_ROTATION || mode == REOPEN_WORKER_ROTATION_ONLY)
    cr_assert(test_case.rotated_in_worker, "the actual worker rotation path was not exercised");
  cr_assert_eq(log_writer_opened(test_case.writer), open_before_retry);
  if (open_before_retry)
    {
      LogProtoClient *expected_proto = mode == REOPEN_NONE ? test_case.initial_proto
                                       : mode == REOPEN_WORKER_ROTATION_ONLY ? test_case.installed_rotation_proto
                                       : test_case.replacement_proto;
      cr_assert_eq(log_writer_get_proto(test_case.writer), expected_proto,
                   "completion retained the wrong proto");
    }

  /* Permit the one delayed recovery where expected, but reject stale or
   * duplicate retries. No-reopen modes must not invent any retry at all. */
  _run_main_loop_for(2);
  gint expected_retries = !_expects_open_writer(mode) || mode == REOPEN_TIMER_RECOVERY || mode == REOPEN_OWNER_RECOVERY;
  cr_assert_eq(test_case.notify_codes[NC_REOPEN_REQUIRED], expected_retries);
  cr_assert_eq(test_case.notify_codes[NC_CLOSE], work_result == LPS_EOF);
  cr_assert_eq(test_case.notify_codes[NC_WRITE_ERROR], failed && work_result != LPS_EOF);
  cr_assert_eq(test_case.control.notify_count, expected_retries + failed);
  cr_assert_eq(log_writer_opened(test_case.writer), _expects_open_writer(mode));
  if (mode == REOPEN_OWNER_RECOVERY)
    cr_assert_eq(log_writer_get_proto(test_case.writer), test_case.replacement_proto);

  /* No job remains active: stop the writer and unregister test callbacks
   * before clearing the fixture's resources or closing the attached pipe fds. */
  cr_assert(log_pipe_deinit((LogPipe *) test_case.writer));
  log_pipe_unref((LogPipe *) test_case.writer);
  log_pipe_deinit(&test_case.control.super);
  log_queue_unref(queue);
  log_writer_options_destroy(&test_case.options);
  iv_event_unregister(&test_case.blocked);
  if (iv_timer_registered(&test_case.release_timer))
    iv_timer_unregister(&test_case.release_timer);
  if (iv_task_registered(&test_case.synchronous_completion))
    iv_task_unregister(&test_case.synchronous_completion);

  /* Mock transports do not close these fds; the test owns both pipe ends. */
  close(active_fds[0]);
  close(active_fds[1]);
  if (test_case.replacement_fds[0] >= 0)
    {
      close(test_case.replacement_fds[0]);
      close(test_case.replacement_fds[1]);
    }
  if (test_case.rotation_fds[0] >= 0)
    {
      close(test_case.rotation_fds[0]);
      close(test_case.rotation_fds[1]);
    }

  /* Pool teardown is asynchronous. Drain its ivykis shutdown events before
   * clearing the synchronization objects and application thread resources. */
  main_loop_call_deinit();
  main_loop_io_worker_deinit();
  iv_main();
  main_loop_worker_deinit();
  g_cond_clear(&test_case.cond);
  g_mutex_clear(&test_case.lock);
  threaded_reopen_case = NULL;
  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}

static void
_assert_threaded_reopen_result(ThreadedReopenMode mode, LogProtoStatus work_result)
{
  _assert_writer_reopen_io(mode, work_result, FALSE, TRUE);
}

static void
_assert_threaded_reopen(ThreadedReopenMode mode)
{
  _assert_threaded_reopen_result(mode, LPS_SUCCESS);
}

Test(logwriter, deferred_null_reopen_notifies_once, .timeout = 20)
{
  /* Defer a NULL proto while post() is blocked, then allow successful I/O
   * completion. The pending close must leave the writer disconnected and
   * notify its owner with NC_REOPEN_REQUIRED exactly once. Pumping the loop
   * beyond the old retry deadline also checks that completion consumed the
   * timer rather than leaving a second reconnect notification scheduled. */
  _assert_threaded_reopen(REOPEN_NULL);
}

Test(logwriter, repeated_deferred_null_reopen_notifies_once, .timeout = 20)
{
  /* Submit two NULL reopen requests during the same blocked I/O job. They
   * must converge on one pending close, not two reconnect notifications.
   * After completion the writer must be disconnected, and running past the
   * retry deadline must not produce another notification from either request. */
  _assert_threaded_reopen(REOPEN_NULL_REPEATED);
}

Test(logwriter, valid_pending_proto_cancels_retry_before_completion, .timeout = 20)
{
  /* Arm a retry with a deferred NULL reopen, then supersede it with a valid
   * replacement while the worker remains blocked. Hold the worker beyond
   * the original retry deadline: accepting the replacement must already
   * suppress that timer, without waiting for completion to install the proto.
   * Completion must leave the writer open, with no reconnect notifications
   * before or after installation of the replacement. */
  _assert_threaded_reopen(REOPEN_VALID_PENDING);
}

Test(logwriter, worker_rotation_does_not_touch_main_thread_retry_timer, .timeout = 20)
{
  /* While post() is blocked, defer a NULL reopen and arm its main-thread
   * retry timer. Resume I/O and supply a valid proto through NC_LOGROTATE,
   * exercising log_writer_logrotate() and its setter in the actual worker.
   * The setter must not unregister the timer from the worker's ivykis heap.
   * The pending NULL request still wins at completion: the writer closes and
   * emits exactly one main-thread reconnect notification, without a stale
   * timer notification later. This tests writer rotation, not filesystem rotation. */
  _assert_threaded_reopen(REOPEN_WORKER_ROTATION);
}

/* Exercise the four completion outcomes across the listed proto transitions.
 * Each expansion is an independent Criterion test, not a pointer-bearing
 * parameter. Except in explicit owner-recovery mode, the mock owner only counts
 * EOF/error: it does not close the proto. These are writer-state assertions,
 * not a claim that every real destination retains its connection on failure. */
#define REOPEN_OUTCOME_TEST(name, mode, result) \
  Test(logwriter, name, .timeout = 20) \
  { \
    _assert_threaded_reopen_result(mode, result); \
  }

REOPEN_OUTCOME_TEST(no_reopen_success, REOPEN_NONE, LPS_SUCCESS)
REOPEN_OUTCOME_TEST(no_reopen_partial, REOPEN_NONE, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(no_reopen_eof, REOPEN_NONE, LPS_EOF)
REOPEN_OUTCOME_TEST(no_reopen_error, REOPEN_NONE, LPS_ERROR)

REOPEN_OUTCOME_TEST(direct_valid_pending_success, REOPEN_VALID_ONLY, LPS_SUCCESS)
REOPEN_OUTCOME_TEST(direct_valid_pending_partial, REOPEN_VALID_ONLY, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(direct_valid_pending_eof, REOPEN_VALID_ONLY, LPS_EOF)
REOPEN_OUTCOME_TEST(direct_valid_pending_error, REOPEN_VALID_ONLY, LPS_ERROR)

REOPEN_OUTCOME_TEST(deferred_null_partial, REOPEN_NULL, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(deferred_null_eof, REOPEN_NULL, LPS_EOF)
REOPEN_OUTCOME_TEST(deferred_null_error, REOPEN_NULL, LPS_ERROR)
REOPEN_OUTCOME_TEST(repeated_deferred_null_partial, REOPEN_NULL_REPEATED, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(repeated_deferred_null_eof, REOPEN_NULL_REPEATED, LPS_EOF)
REOPEN_OUTCOME_TEST(repeated_deferred_null_error, REOPEN_NULL_REPEATED, LPS_ERROR)

REOPEN_OUTCOME_TEST(valid_pending_partial, REOPEN_VALID_PENDING, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(valid_pending_eof, REOPEN_VALID_PENDING, LPS_EOF)
REOPEN_OUTCOME_TEST(valid_pending_error, REOPEN_VALID_PENDING, LPS_ERROR)

REOPEN_OUTCOME_TEST(rotation_with_pending_close_partial, REOPEN_WORKER_ROTATION, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(rotation_with_pending_close_eof, REOPEN_WORKER_ROTATION, LPS_EOF)
REOPEN_OUTCOME_TEST(rotation_with_pending_close_error, REOPEN_WORKER_ROTATION, LPS_ERROR)
REOPEN_OUTCOME_TEST(rotation_without_reopen_success, REOPEN_WORKER_ROTATION_ONLY, LPS_SUCCESS)
REOPEN_OUTCOME_TEST(rotation_without_reopen_partial, REOPEN_WORKER_ROTATION_ONLY, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(rotation_without_reopen_eof, REOPEN_WORKER_ROTATION_ONLY, LPS_EOF)
REOPEN_OUTCOME_TEST(rotation_without_reopen_error, REOPEN_WORKER_ROTATION_ONLY, LPS_ERROR)

/* Let the actual retry timer expire while post() is still blocked. The
 * owner's successful response must become pending, install at completion,
 * and never be followed by a second NC_REOPEN_REQUIRED notification. */
REOPEN_OUTCOME_TEST(timer_recovery_before_completion_success, REOPEN_TIMER_RECOVERY, LPS_SUCCESS)
REOPEN_OUTCOME_TEST(timer_recovery_before_completion_partial, REOPEN_TIMER_RECOVERY, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(timer_recovery_before_completion_eof, REOPEN_TIMER_RECOVERY, LPS_EOF)
REOPEN_OUTCOME_TEST(timer_recovery_before_completion_error, REOPEN_TIMER_RECOVERY, LPS_ERROR)

/* Exercise synchronous owner callbacks re-entering reopen(), including an
 * error owner that first closes and later reopens when its retry expires. */
REOPEN_OUTCOME_TEST(owner_reopens_during_completion_success, REOPEN_OWNER_RECOVERY, LPS_SUCCESS)
REOPEN_OUTCOME_TEST(owner_reopens_during_completion_partial, REOPEN_OWNER_RECOVERY, LPS_PARTIAL)
REOPEN_OUTCOME_TEST(owner_recovers_after_eof, REOPEN_OWNER_RECOVERY, LPS_EOF)
REOPEN_OUTCOME_TEST(owner_recovers_after_error, REOPEN_OWNER_RECOVERY, LPS_ERROR)

#undef REOPEN_OUTCOME_TEST

/* Input jobs share completion with output jobs, but enter through process_in
 * rather than post/flush. Exercise both directions through their real fd watches. */
#define REOPEN_INPUT_TEST(name, mode, result) \
  Test(logwriter, name, .timeout = 20) \
  { \
    _assert_writer_reopen_io(mode, result, TRUE, TRUE); \
  }

REOPEN_INPUT_TEST(input_without_reopen_success, REOPEN_NONE, LPS_SUCCESS)
REOPEN_INPUT_TEST(input_without_reopen_partial, REOPEN_NONE, LPS_PARTIAL)
REOPEN_INPUT_TEST(input_without_reopen_eof, REOPEN_NONE, LPS_EOF)
REOPEN_INPUT_TEST(input_without_reopen_error, REOPEN_NONE, LPS_ERROR)
REOPEN_INPUT_TEST(input_deferred_null_success, REOPEN_NULL, LPS_SUCCESS)
REOPEN_INPUT_TEST(input_deferred_null_partial, REOPEN_NULL, LPS_PARTIAL)
REOPEN_INPUT_TEST(input_deferred_null_eof, REOPEN_NULL, LPS_EOF)
REOPEN_INPUT_TEST(input_deferred_null_error, REOPEN_NULL, LPS_ERROR)
REOPEN_INPUT_TEST(input_valid_pending_success, REOPEN_VALID_PENDING, LPS_SUCCESS)
REOPEN_INPUT_TEST(input_valid_pending_partial, REOPEN_VALID_PENDING, LPS_PARTIAL)
REOPEN_INPUT_TEST(input_valid_pending_eof, REOPEN_VALID_PENDING, LPS_EOF)
REOPEN_INPUT_TEST(input_valid_pending_error, REOPEN_VALID_PENDING, LPS_ERROR)
REOPEN_INPUT_TEST(input_timer_recovery_success, REOPEN_TIMER_RECOVERY, LPS_SUCCESS)
REOPEN_INPUT_TEST(input_timer_recovery_partial, REOPEN_TIMER_RECOVERY, LPS_PARTIAL)
REOPEN_INPUT_TEST(input_timer_recovery_eof, REOPEN_TIMER_RECOVERY, LPS_EOF)
REOPEN_INPUT_TEST(input_timer_recovery_error, REOPEN_TIMER_RECOVERY, LPS_ERROR)

#undef REOPEN_INPUT_TEST

/* Non-threaded writers call the same completion code synchronously. With a
 * counting owner that does not close on EOF/error, the proto stays installed;
 * no outcome may invent an NC_REOPEN_REQUIRED notification without a request. */
#define SYNCHRONOUS_OUTCOME_TEST(name, result, input) \
  Test(logwriter, name, .timeout = 20) \
  { \
    _assert_writer_reopen_io(REOPEN_NONE, result, input, FALSE); \
  }

SYNCHRONOUS_OUTCOME_TEST(synchronous_output_success, LPS_SUCCESS, FALSE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_output_partial, LPS_PARTIAL, FALSE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_output_eof, LPS_EOF, FALSE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_output_error, LPS_ERROR, FALSE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_input_success, LPS_SUCCESS, TRUE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_input_partial, LPS_PARTIAL, TRUE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_input_eof, LPS_EOF, TRUE)
SYNCHRONOUS_OUTCOME_TEST(synchronous_input_error, LPS_ERROR, TRUE)

#undef SYNCHRONOUS_OUTCOME_TEST

typedef enum
{
  CLOSE_OPEN_WRITER,
  DEINIT_PENDING_RETRY,
  REINIT_PENDING_RETRY,
  STEAL_AND_RESTORE_PROTO,
} ReopenLifecycle;

static void
_assert_reopen_lifecycle(ReopenLifecycle lifecycle, gint time_reopen)
{
  configuration = cfg_new_snippet();
  app_startup();
  LogWriterOptions opt = {0};
  log_writer_options_defaults(&opt);
  log_writer_options_init(&opt, configuration, LWO_NO_STATS);
  opt.time_reopen = time_reopen;
  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  MockControlPipe *control = _mock_control_pipe_new(configuration);
  LogWriter *writer = _construct_writer(configuration, &opt, control, queue);
  gint fds[2] = { -1, -1 };
  LogProtoClient *proto = NULL;

  if (lifecycle == CLOSE_OPEN_WRITER || lifecycle == STEAL_AND_RESTORE_PROTO)
    {
      proto = _new_mock_proto(&opt.proto_options, fds);
      log_writer_reopen(writer, proto);
      cr_assert(log_writer_opened(writer));
    }

  if (lifecycle == STEAL_AND_RESTORE_PROTO)
    {
      /* Model the writer-level kept-alive handoff used during reload: stop,
       * steal, reinitialize, restore. This is not a full destination reload. */
      cr_assert(log_pipe_deinit((LogPipe *) writer));
      cr_assert_eq(log_writer_steal_proto(writer), proto);
      cr_assert_not(log_writer_opened(writer));
      cr_assert(log_pipe_init((LogPipe *) writer));
      log_writer_reopen(writer, proto);
    }
  else
    {
      /* CLOSE_OPEN_WRITER closes an idle open writer; other modes arm a retry
       * while already closed, then deinitialize or reinitialize. Teardown must
       * preserve a new close retry but cancel it when the writer is deinitialized. */
      log_writer_reopen(writer, NULL);
      cr_assert_not(log_writer_opened(writer));
      if (lifecycle != CLOSE_OPEN_WRITER)
        cr_assert(log_pipe_deinit((LogPipe *) writer));
      if (lifecycle == REINIT_PENDING_RETRY)
        {
          cr_assert(log_pipe_init((LogPipe *) writer));
          proto = _new_mock_proto(&opt.proto_options, fds);
          log_writer_reopen(writer, proto);
        }
    }

  _run_main_loop_for(2);
  cr_assert_eq(control->notify_count, lifecycle == CLOSE_OPEN_WRITER ? 1 : 0);
  if (lifecycle == CLOSE_OPEN_WRITER)
    cr_assert_eq(control->last_notify_code, NC_REOPEN_REQUIRED);
  if (lifecycle == REINIT_PENDING_RETRY || lifecycle == STEAL_AND_RESTORE_PROTO)
    cr_assert_eq(log_writer_get_proto(writer), proto);

  cr_assert(log_pipe_deinit((LogPipe *) writer));
  log_pipe_unref((LogPipe *) writer);
  log_pipe_unref(&control->super);
  log_queue_unref(queue);
  log_writer_options_destroy(&opt);
  if (fds[0] >= 0)
    {
      close(fds[0]);
      close(fds[1]);
    }
  app_shutdown();
  iv_deinit();
  cfg_free(configuration);
}

Test(logwriter, closing_an_open_idle_writer_preserves_retry)
{
  _assert_reopen_lifecycle(CLOSE_OPEN_WRITER, 1);
}

Test(logwriter, deinit_cancels_retry_without_running_watches)
{
  _assert_reopen_lifecycle(DEINIT_PENDING_RETRY, 1);
}

Test(logwriter, reinit_does_not_inherit_a_stale_retry)
{
  _assert_reopen_lifecycle(REINIT_PENDING_RETRY, 1);
}

Test(logwriter, kept_alive_proto_survives_steal_and_restore_without_retry)
{
  _assert_reopen_lifecycle(STEAL_AND_RESTORE_PROTO, 1);
}

/* An immediately due retry must survive close but be canceled by deinit.
 * Reinitializing with a valid proto must not inherit it before loop dispatch. */
Test(logwriter, zero_delay_close_preserves_retry)
{
  _assert_reopen_lifecycle(CLOSE_OPEN_WRITER, 0);
}

Test(logwriter, zero_delay_deinit_cancels_retry)
{
  _assert_reopen_lifecycle(DEINIT_PENDING_RETRY, 0);
}

Test(logwriter, zero_delay_reinit_does_not_inherit_retry)
{
  _assert_reopen_lifecycle(REINIT_PENDING_RETRY, 0);
}

typedef struct
{
  LogWriter *writer;
  LogProtoClient *proto;
  struct iv_event returned;
  gboolean completed;
} ExternalReopen;

static gpointer
_reopen_from_external_thread(gpointer user_data)
{
  ExternalReopen *request = user_data;

  main_loop_call_thread_init();
  log_writer_reopen(request->writer, request->proto);
  main_loop_call_thread_deinit();
  iv_event_post(&request->returned);
  return NULL;
}

static void
_external_reopen_returned(gpointer user_data)
{
  ExternalReopen *request = user_data;

  request->completed = TRUE;
  iv_quit();
}

static void
_assert_external_reopen(gboolean valid)
{
  app_startup();
  configuration = cfg_new_snippet();
  main_loop_call_init();
  LogWriterOptions opt = {0};
  log_writer_options_defaults(&opt);
  log_writer_options_init(&opt, configuration, LWO_NO_STATS);
  opt.time_reopen = 1;
  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  MockControlPipe *control = _mock_control_pipe_new(configuration);
  LogWriter *writer = _construct_writer(configuration, &opt, control, queue);
  ExternalReopen request = { .writer = writer };
  gint fds[2] = { -1, -1 };
  if (valid)
    {
      log_writer_reopen(writer, NULL);
      request.proto = _new_mock_proto(&opt.proto_options, fds);
    }
  IV_EVENT_INIT(&request.returned);
  request.returned.cookie = &request;
  request.returned.handler = _external_reopen_returned;
  iv_event_register(&request.returned);

  /* Exercise a non-main caller through the real synchronous dispatcher while
   * no I/O job is active. Timer ownership stays on main; this does not test
   * the additional pending-proto condition wait used during active I/O. */
  GThread *caller = g_thread_new("writer-reopen", _reopen_from_external_thread, &request);
  _run_main_loop_for(10);
  cr_assert(request.completed, "the external reopen caller did not return");
  g_thread_join(caller);
  cr_assert_eq(log_writer_get_proto(writer), request.proto);
  _run_main_loop_for(2);
  cr_assert_eq(control->notify_count, valid ? 0 : 1);
  if (!valid)
    cr_assert_eq(control->last_notify_code, NC_REOPEN_REQUIRED);

  cr_assert(log_pipe_deinit((LogPipe *) writer));
  log_pipe_unref((LogPipe *) writer);
  log_pipe_unref(&control->super);
  log_queue_unref(queue);
  log_writer_options_destroy(&opt);
  iv_event_unregister(&request.returned);
  main_loop_call_deinit();
  if (valid)
    {
      close(fds[0]);
      close(fds[1]);
    }
  cfg_free(configuration);
  app_shutdown();
  iv_deinit();
}

Test(logwriter, external_thread_valid_reopen_cancels_main_thread_retry, .timeout = 20)
{
  _assert_external_reopen(TRUE);
}

Test(logwriter, external_thread_null_reopen_arms_main_thread_retry, .timeout = 20)
{
  _assert_external_reopen(FALSE);
}
