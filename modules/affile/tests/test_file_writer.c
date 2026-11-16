/*
 * Copyright (c) 2019 Balazs Scheidler <bazsi77@gmail.com>
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
#include "libtest/cr_template.h"
#include "libtest/mock-transport.h"

#include "logproto-file-writer.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "affile-dest.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <iv.h>


static void _ack_callback(gint num_acked, gpointer user_data);

/* helper variables used by all testcases below */

static LogProtoClientOptionsStorage options = {0};
static LogMessage *msg;
static LogTransport *transport;
static LogProtoClientFlowControlFuncs flow_control_funcs =
{
  .ack_callback = _ack_callback,
};
static gchar output_buffer[8192];
static LogProtoStatus status;
static gint messages_acked = 0;
static gboolean consumed;
static const gchar *payload = "PAYLOAD";
static gssize count;

static void
_ack_callback(gint num_acked, gpointer user_data)
{
  messages_acked += num_acked;
}

Test(file_writer, write_single_message_and_flush_is_expected_to_dump_the_payload_to_the_output)
{
  LogProtoClient *fw = log_proto_file_writer_new(transport, &options, 100, FALSE);

  log_proto_client_set_client_flow_control(fw, &flow_control_funcs);

  status = log_proto_client_post(fw, msg, (guchar *) g_strdup(payload), strlen(payload) + 1, &consumed);
  cr_assert(status == LPS_SUCCESS);
  cr_assert(consumed == TRUE);

  status = log_proto_client_flush(fw);
  cr_assert(status == LPS_SUCCESS);

  log_transport_mock_read_from_write_buffer((LogTransportMock *) transport, output_buffer, sizeof(output_buffer));
  cr_assert_str_eq(output_buffer, "PAYLOAD");
  cr_assert(messages_acked == 1);

  log_proto_client_free(fw);
}

Test(file_writer, batches_of_messages_should_be_flushed_to_the_output)
{
  const gint BATCH_SIZE = 10;
  const gint MESSAGE_COUNT = BATCH_SIZE * 3;
  LogProtoClient *fw = log_proto_file_writer_new(transport, &options, BATCH_SIZE, FALSE);

  log_proto_client_set_client_flow_control(fw, &flow_control_funcs);
  for (gint i = 0; i < MESSAGE_COUNT; i++)
    {
      status = log_proto_client_post(fw, msg, (guchar *) g_strdup(payload), strlen(payload) + 1, &consumed);
      cr_assert(status == LPS_SUCCESS);
      cr_assert(consumed == TRUE);
    }

  status = log_proto_client_flush(fw);
  cr_assert(status == LPS_SUCCESS);

  count = log_transport_mock_read_from_write_buffer((LogTransportMock *) transport, output_buffer, sizeof(output_buffer));
  cr_assert_eq(count, MESSAGE_COUNT * (strlen(payload) + 1));

  for (gint i = 0; i < MESSAGE_COUNT; i++)
    {
      const gchar *output_element = output_buffer + i * (strlen(payload) + 1);
      cr_assert_str_eq(output_element, "PAYLOAD");
    }
  cr_assert_eq(messages_acked, MESSAGE_COUNT);

  log_proto_client_free(fw);
}

Test(file_writer, messages_should_be_flushed_automatically_once_we_reach_batch_size)
{
  const gint BATCH_SIZE = 10;
  LogProtoClient *fw = log_proto_file_writer_new(transport, &options, BATCH_SIZE, FALSE);

  log_proto_client_set_client_flow_control(fw, &flow_control_funcs);
  for (gint i = 0; i < BATCH_SIZE - 1; i++)
    {
      status = log_proto_client_post(fw, msg, (guchar *) g_strdup(payload), strlen(payload) + 1, &consumed);
      cr_assert(status == LPS_SUCCESS);
      cr_assert(consumed == TRUE);
    }

  count = log_transport_mock_read_from_write_buffer((LogTransportMock *) transport, output_buffer, sizeof(output_buffer));
  cr_assert_eq(count, 0);

  /* Push an extra item, which causes the BATCH_SIZE limit to be reached. There's an automatic flush in this case. */
  status = log_proto_client_post(fw, msg, (guchar *) g_strdup(payload), strlen(payload) + 1, &consumed);
  cr_assert(status == LPS_SUCCESS);
  cr_assert(consumed == TRUE);

  count = log_transport_mock_read_from_write_buffer((LogTransportMock *) transport, output_buffer, sizeof(output_buffer));
  cr_assert_eq(count, BATCH_SIZE * (strlen(payload) + 1));
  cr_assert_eq(messages_acked, BATCH_SIZE);

  log_proto_client_free(fw);
}

Test(file_writer, batches_of_messages_are_flushed_even_if_the_underlying_transport_is_accepting_a_few_bytes_per_write)
{
  const gint BATCH_SIZE = 10;
  LogProtoClient *fw = log_proto_file_writer_new(transport, &options, BATCH_SIZE, FALSE);

  log_transport_mock_set_write_chunk_limit((LogTransportMock *) transport, 2);
  log_proto_client_set_client_flow_control(fw, &flow_control_funcs);
  for (gint i = 0; i < BATCH_SIZE; i++)
    {
      status = log_proto_client_post(fw, msg, (guchar *) g_strdup(payload), strlen(payload) + 1, &consumed);
      cr_assert(status == LPS_SUCCESS, "status=%d", status);
      cr_assert(consumed == TRUE);
    }

  while ((status = log_proto_client_flush(fw)) == LPS_PARTIAL)
    ;

  cr_assert(status == LPS_SUCCESS);

  count = log_transport_mock_read_from_write_buffer((LogTransportMock *) transport, output_buffer, sizeof(output_buffer));
  cr_assert_eq(count, BATCH_SIZE * (strlen(payload) + 1));

  for (gint i = 0; i < BATCH_SIZE; i++)
    {
      const gchar *output_element = output_buffer + i * (strlen(payload) + 1);
      cr_assert_str_eq(output_element, "PAYLOAD");
    }
  cr_assert_eq(messages_acked, BATCH_SIZE);

  log_proto_client_free(fw);
}

static gint destination_open_attempts;
static gint (*destination_open)(FileOpener *, const gchar *, gint);

static gint
_count_destination_opens(FileOpener *opener, const gchar *name, gint flags)
{
  /* Inject only the initial failure; subsequent attempts use the original
   * opener and real filesystem, while retaining an observable attempt count. */
  destination_open_attempts++;
  if (destination_open_attempts == 1)
    {
      errno = EACCES;
      return -1;
    }
  return destination_open(opener, name, flags);
}

static void
_stop_file_reopen_loop(gpointer user_data)
{
  iv_quit();
}

/* Exercise the real affile destination and file opener without active I/O
 * deferral or rotation. The threaded writer tests cover those paths separately. */
Test(affile_reopen, successful_reopen_does_not_open_the_file_twice, .timeout = 10)
{
  app_startup();
  configuration = cfg_new_snippet();
  configuration->threaded = FALSE;
  GError *error = NULL;
  gchar *directory = g_dir_make_tmp("syslog-ng-reopen-XXXXXX", &error);
  cr_assert_not_null(directory, "%s", error ? error->message : "temporary directory creation failed");
  gchar *filename = g_build_filename(directory, "destination.log", NULL);
  LogTemplate *filename_template = log_template_new(configuration, NULL);
  cr_assert(log_template_compile(filename_template, filename, &error));
  LogDriver *destination = affile_dd_new(filename_template, configuration);
  AFFileDestDriver *driver = (AFFileDestDriver *) destination;
  driver->writer_options.time_reopen = 1;
  driver->writer_options.options |= LWO_NO_STATS;
  destination_open = driver->file_opener->open;
  driver->file_opener->open = _count_destination_opens;
  destination_open_attempts = 0;
  cr_assert(log_pipe_init((LogPipe *) destination));

  /* Queuing creates the real AFFileDestWriter. The injected initial failure
   * makes affile request a NULL proto, arming the closed writer's retry timer. */
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  path_options.ack_needed = FALSE;
  LogMessage *message = log_msg_new_empty();
  log_msg_set_value(message, LM_V_MESSAGE, "destination reopen regression", -1);
  log_pipe_queue((LogPipe *) destination, message, &path_options);
  cr_assert_not_null(driver->single_writer);
  LogPipe *file_writer = (LogPipe *) driver->single_writer;
  LogWriter *writer = (LogWriter *) file_writer->pipe_next;
  cr_assert_not_null(writer);
  cr_assert_not(log_writer_opened(writer));
  cr_assert_eq(destination_open_attempts, 1);

  /* Keep the writer idle: another I/O job would stop its watches and could
   * cancel the stale timer, hiding a missing cancellation during reopen. */
  LogQueue *queue = log_writer_get_queue(writer);
  LogMessage *held_message = log_queue_pop_head(queue, &path_options);
  cr_assert_not_null(held_message);
  log_queue_ack_backlog(queue, 1);
  log_queue_unref(queue);

  /* Invoke the actual retry handler directly before the scheduled timer fires.
   * Its successful open supplies a valid proto through log_writer_reopen(),
   * which must cancel the obsolete retry in its main-thread request path. */
  log_pipe_notify(file_writer, NC_REOPEN_REQUIRED, writer);
  cr_assert(log_writer_opened(writer));
  cr_assert_eq(destination_open_attempts, 2);
  LogProtoClient *opened_proto = log_writer_get_proto(writer);
  struct iv_timer stop_timer;
  IV_TIMER_INIT(&stop_timer);
  stop_timer.handler = _stop_file_reopen_loop;
  iv_validate_now();
  stop_timer.expires = iv_now;
  stop_timer.expires.tv_sec += 2;
  iv_timer_register(&stop_timer);
  iv_main();

  /* Expect one injected failure and one successful open. A stale timer would
   * invoke affile_dw_notify() again and cause a third open attempt. Proto
   * identity supplements the attempt count; it cannot replace that count. */
  cr_assert_eq(destination_open_attempts, 2, "a stale writer retry reopened the destination file");
  cr_assert_eq(log_writer_get_proto(writer), opened_proto);

  /* Only after the stale-timer check, let the real destination process its
   * initial protocol handshake and write the held message. Deinit then flushes
   * any remaining output before file contents are inspected. */
  log_pipe_queue((LogPipe *) destination, held_message, &path_options);
  iv_validate_now();
  stop_timer.expires = iv_now;
  stop_timer.expires.tv_sec += 1;
  iv_timer_register(&stop_timer);
  iv_main();
  cr_assert(log_pipe_deinit((LogPipe *) destination));
  gchar *contents = NULL;
  cr_assert(g_file_get_contents(filename, &contents, NULL, &error));
  cr_assert_not_null(strstr(contents, "destination reopen regression"));
  g_free(contents);

  log_pipe_unref((LogPipe *) destination);
  cfg_free(configuration);
  cr_assert_eq(g_unlink(filename), 0);
  cr_assert_eq(g_rmdir(directory), 0);
  g_free(filename);
  g_free(directory);
  app_shutdown();
  iv_deinit();
}

static void
startup(void)
{
  app_startup();
  msg = create_empty_message();
  transport = log_transport_mock_stream_new(NULL, 0);
}

static void
teardown(void)
{
  log_msg_unref(msg);
  app_shutdown();
}

TestSuite(file_writer, .init = startup, .fini = teardown);
