/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "logproto-dgram-server.h"
#include "logproto-buffered-server.h"

/* proto that reads the input in datagrams (e.g. the underlying transport
 * determines record sizes, such as UDP) */
typedef struct _LogProtoDGramServer LogProtoDGramServer;
struct _LogProtoDGramServer
{
  LogProtoBufferedServer super;
};

static gboolean
log_proto_dgram_server_fetch_from_buffer(LogProtoBufferedServer *s, const guchar *buffer_start, gsize buffer_bytes,
                                         const guchar **msg, gsize *msg_len)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(s);

  /*
   * we are set to packet terminating mode
   */
  *msg = buffer_start;
  *msg_len = buffer_bytes;
  state->pending_buffer_pos = state->pending_buffer_end;
  log_proto_buffered_server_put_state(s);
  return TRUE;
}

LogProtoServer *
log_proto_dgram_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoDGramServer *self = g_new0(LogProtoDGramServer, 1);

  log_proto_buffered_server_init(&self->super, transport, options);
  self->super.fetch_from_buffer = log_proto_dgram_server_fetch_from_buffer;
  self->super.stream_based = FALSE;
  return &self->super.super;
}
