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
#include "logproto-record-server.h"
#include "logproto-buffered-server.h"
#include "messages.h"

#include <errno.h>

/* proto that reads the stream in even sized chunks */
typedef struct _LogProtoRecordServer LogProtoRecordServer;
struct _LogProtoRecordServer
{
  LogProtoBufferedServer super;
  gint record_size;
};

static gboolean
log_proto_record_server_validate_options(LogProtoServer *s)
{
  LogProtoRecordServer *self = (LogProtoRecordServer *) s;

  if (s->options->max_buffer_size < self->record_size)
    {
      msg_error("Buffer is too small to hold the number of bytes required for a record, please make sure log-msg-size() is greater than equal to record-size",
                evt_tag_int("record_size", self->record_size),
                evt_tag_int("max_buffer_size", s->options->max_buffer_size));
      return FALSE;
    }
  return log_proto_buffered_server_validate_options_method(s);
}

static gint
log_proto_record_server_read_data(LogProtoBufferedServer *s, guchar *buf, gsize len, LogTransportAuxData *aux)
{
  LogProtoRecordServer *self = (LogProtoRecordServer *) s;
  gint rc;

  /* assert that we have enough space in the buffer to read record_size bytes */
  g_assert(len >= self->record_size);
  len = self->record_size;
  rc = log_transport_read(self->super.super.transport, buf, len, aux);
  if (rc > 0 && rc != self->record_size)
    {
      msg_error("Record size was set, and couldn't read enough bytes",
                evt_tag_int(EVT_TAG_FD, self->super.super.transport->fd),
                evt_tag_int("record_size", self->record_size),
                evt_tag_int("read", rc));
      errno = EIO;
      return -1;
    }
  return rc;
}

static void
log_proto_record_server_init(LogProtoRecordServer *self, LogTransport *transport, const LogProtoServerOptions *options,
                             gint record_size)
{
  log_proto_buffered_server_init(&self->super, transport, options);
  self->super.super.validate_options = log_proto_record_server_validate_options;
  self->super.read_data = log_proto_record_server_read_data;
  self->super.stream_based = FALSE;
  self->record_size = record_size;
}

static gboolean
log_proto_binary_record_server_fetch_from_buffer(LogProtoBufferedServer *s, const guchar *buffer_start,
                                                 gsize buffer_bytes, const guchar **msg, gsize *msg_len)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(s);

  *msg_len = buffer_bytes;

  state->pending_buffer_pos = state->pending_buffer_end;
  *msg = buffer_start;
  log_proto_buffered_server_put_state(s);
  return TRUE;
}

LogProtoServer *
log_proto_binary_record_server_new(LogTransport *transport, const LogProtoServerOptions *options, gint record_size)
{
  LogProtoRecordServer *self = g_new0(LogProtoRecordServer, 1);

  log_proto_record_server_init(self, transport, options, record_size);
  self->super.fetch_from_buffer = log_proto_binary_record_server_fetch_from_buffer;
  return &self->super.super;
}

static gboolean
log_proto_padded_record_server_fetch_from_buffer(LogProtoBufferedServer *s, const guchar *buffer_start,
                                                 gsize buffer_bytes, const guchar **msg, gsize *msg_len)
{
  LogProtoBufferedServerState *state = log_proto_buffered_server_get_state(s);
  const guchar *eol;

  eol = find_eom(buffer_start, buffer_bytes);
  *msg_len = (eol ? eol - buffer_start : buffer_bytes);

  state->pending_buffer_pos = state->pending_buffer_end;
  *msg = buffer_start;
  log_proto_buffered_server_put_state(s);
  return TRUE;
}

LogProtoServer *
log_proto_padded_record_server_new(LogTransport *transport, const LogProtoServerOptions *options, gint record_size)
{
  LogProtoRecordServer *self = g_new0(LogProtoRecordServer, 1);

  log_proto_record_server_init(self, transport, options, record_size);
  self->super.fetch_from_buffer = log_proto_padded_record_server_fetch_from_buffer;
  return &self->super.super;
}
