/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "proto_lib.h"
#include "msg_parse_lib.h"

LogProtoServerOptions proto_server_options;

void
assert_proto_server_status(LogProtoServer *proto, LogProtoStatus status, LogProtoStatus expected_status)
{
  assert_gint(status, expected_status, "LogProtoServer expected status mismatch");
}

LogProtoStatus
proto_server_fetch(LogProtoServer *proto, const guchar **msg, gsize *msg_len)
{
  Bookmark bookmark;
  LogTransportAuxData aux;
  GSockAddr *saddr;
  gboolean may_read = TRUE;
  LogProtoStatus status;

  start_grabbing_messages();
  do
    {
      log_transport_aux_data_init(&aux);
      status = log_proto_server_fetch(proto, msg, msg_len, &may_read, &aux, &bookmark);
    }
  while (status == LPS_SUCCESS && *msg == NULL && may_read);

  saddr = aux.peer_addr;
  if (status == LPS_SUCCESS)
    {
      g_sockaddr_unref(saddr);
    }
  else
    {
      assert_true(saddr == NULL, "returned saddr must be NULL on failure");
    }
  stop_grabbing_messages();
  return status;
}

LogProtoServer *
construct_server_proto_plugin(const gchar *name, LogTransport *transport)
{
  LogProtoServerFactory *proto_factory;

  log_proto_server_options_init(&proto_server_options, configuration);
  proto_factory = log_proto_server_get_factory(&configuration->plugin_context, name);
  assert_true(proto_factory != NULL, "error looking up proto factory");
  return log_proto_server_factory_construct(proto_factory, transport, &proto_server_options);
}

void
assert_proto_server_fetch(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len)
{
  const guchar *msg = NULL;
  gsize msg_len = 0;
  LogProtoStatus status;

  status = proto_server_fetch(proto, &msg, &msg_len);

  assert_proto_server_status(proto, status, LPS_SUCCESS);
  assert_nstring((const gchar *) msg, msg_len, expected_msg, expected_msg_len,
                 "LogProtoServer expected message mismatch");
}

void
assert_proto_server_fetch_single_read(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len)
{
  const guchar *msg = NULL;
  gsize msg_len = 0;
  LogProtoStatus status;
  LogTransportAuxData aux;
  Bookmark bookmark;
  gboolean may_read = TRUE;

  start_grabbing_messages();
  log_transport_aux_data_init(&aux);
  status = log_proto_server_fetch(proto, &msg, &msg_len, &may_read, &aux, &bookmark);
  assert_proto_server_status(proto, status, LPS_SUCCESS);

  if (expected_msg)
    {
      assert_nstring((const gchar *) msg, msg_len, expected_msg, expected_msg_len,
                     "LogProtoServer expected message mismatch");
    }
  else
    {
      assert_true(msg == NULL, "when single-read finds an incomplete message, msg must be NULL");
      assert_true(aux.peer_addr == NULL, "returned saddr must be NULL on success");
    }
  stop_grabbing_messages();
}

void
assert_proto_server_fetch_failure(LogProtoServer *proto, LogProtoStatus expected_status, const gchar *error_message)
{
  const guchar *msg = NULL;
  gsize msg_len = 0;
  LogProtoStatus status;

  status = proto_server_fetch(proto, &msg, &msg_len);

  assert_proto_server_status(proto, status, expected_status);
  if (error_message)
    assert_grabbed_messages_contain(error_message, "expected error message didn't show up");
}

void
assert_proto_server_fetch_ignored_eof(LogProtoServer *proto)
{
  const guchar *msg = NULL;
  gsize msg_len = 0;
  LogProtoStatus status;
  LogTransportAuxData aux;
  Bookmark bookmark;
  gboolean may_read = TRUE;

  start_grabbing_messages();
  log_transport_aux_data_init(&aux);
  status = log_proto_server_fetch(proto, &msg, &msg_len, &may_read, &aux, &bookmark);
  assert_proto_server_status(proto, status, LPS_SUCCESS);
  assert_true(msg == NULL, "when an EOF is ignored msg must be NULL");
  assert_true(aux.peer_addr == NULL, "returned saddr must be NULL on success");
  stop_grabbing_messages();
}

void
init_proto_tests(void)
{
  init_and_load_syslogformat_module();
  log_proto_server_options_defaults(&proto_server_options);
  log_proto_server_options_init(&proto_server_options, configuration);

}

void
deinit_proto_tests(void)
{
  deinit_syslogformat_module();
}
