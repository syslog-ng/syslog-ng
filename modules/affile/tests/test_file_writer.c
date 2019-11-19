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
#include "logproto-file-writer.h"
#include "logmsg/logmsg.h"
#include "apphook.h"

#include <criterion/criterion.h>
#include "cr_template.h"
#include "mock-transport.h"

static void _ack_callback(gint num_acked, gpointer user_data);

/* helper variables used by all testcases below */

static LogProtoClientOptions options = {0};
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
