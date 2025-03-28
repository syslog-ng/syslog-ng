/*
 * Copyright (c) 2012-2019 Balabit
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

#include <criterion/criterion.h>
#include "proto_lib.h"
#include "grab-logging.h"

#include "cfg.h"
#include <string.h>

LogProtoServerOptions proto_server_options;

void
assert_proto_server_status(LogProtoServer *proto, LogProtoStatus status, LogProtoStatus expected_status)
{
  cr_assert_eq(status, expected_status, "LogProtoServer expected status mismatch");
}

LogProtoStatus
proto_server_handshake(LogProtoServer **proto)
{
  gboolean handshake_finished = FALSE;
  LogProtoStatus status;

  start_grabbing_messages();
  do
    {
      LogProtoServer *proto_replacement = NULL;
      status = log_proto_server_handshake(*proto, &handshake_finished, &proto_replacement);
      if (status == LPS_AGAIN)
        status = LPS_SUCCESS;
      if (proto_replacement)
        {
          log_transport_stack_move(&proto_replacement->transport_stack, &(*proto)->transport_stack);
          log_proto_server_free(*proto);
          *proto = proto_replacement;
        }
    }
  while (status == LPS_SUCCESS && handshake_finished == FALSE);
  stop_grabbing_messages();
  return status;
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
      if (status == LPS_AGAIN)
        status = LPS_SUCCESS;
    }
  while (status == LPS_SUCCESS && *msg == NULL && may_read);

  saddr = aux.peer_addr;
  if (status == LPS_SUCCESS)
    {
      g_sockaddr_unref(saddr);
    }
  else
    {
      cr_assert_null(saddr, "returned saddr must be NULL on failure");
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
  cr_assert_not_null(proto_factory, "error looking up proto factory");
  return log_proto_server_factory_construct(proto_factory, transport, &proto_server_options);
}

void
assert_proto_server_handshake(LogProtoServer **proto)
{
  LogProtoStatus status;

  status = proto_server_handshake(proto);

  assert_proto_server_status(*proto, status, LPS_SUCCESS);
}

void
assert_proto_server_fetch(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len)
{
  const guchar *msg = NULL;
  gsize msg_len = 0;
  LogProtoStatus status;

  status = proto_server_fetch(proto, &msg, &msg_len);

  assert_proto_server_status(proto, status, LPS_SUCCESS);

  if (expected_msg_len < 0)
    expected_msg_len = strlen(expected_msg);

  cr_assert_eq(msg_len, expected_msg_len, "LogProtoServer expected message mismatch (length) "
               "actual: %" G_GSIZE_FORMAT " expected: %" G_GSIZE_FORMAT, msg_len, expected_msg_len);
  cr_assert_arr_eq((const gchar *) msg, expected_msg, expected_msg_len,
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
      if (expected_msg_len < 0)
        expected_msg_len = strlen(expected_msg);

      cr_assert_eq(msg_len, expected_msg_len, "LogProtoServer expected message mismatch (length)");
      cr_assert_arr_eq((const gchar *) msg, expected_msg, expected_msg_len,
                       "LogProtoServer expected message mismatch");
    }
  else
    {
      cr_assert_null(msg, "when single-read finds an incomplete message, msg must be NULL");
      cr_assert_null(aux.peer_addr, "returned saddr must be NULL on success");
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
    assert_grabbed_log_contains(error_message);
}

void
assert_proto_server_handshake_failure(LogProtoServer **proto, LogProtoStatus expected_status)
{
  LogProtoStatus status;

  status = proto_server_handshake(proto);

  assert_proto_server_status(*proto, status, expected_status);
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
  if (status == LPS_AGAIN)
    status = LPS_SUCCESS;
  assert_proto_server_status(proto, status, LPS_SUCCESS);
  cr_assert_null(msg, "when an EOF is ignored msg must be NULL");
  cr_assert_null(aux.peer_addr, "returned saddr must be NULL on success");
  stop_grabbing_messages();
}

void
init_proto_tests(void)
{
  configuration = cfg_new_snippet();
  log_proto_server_options_defaults(&proto_server_options);
}

void
deinit_proto_tests(void)
{
  log_proto_server_options_destroy(&proto_server_options);

  if (configuration)
    cfg_free(configuration);
}
