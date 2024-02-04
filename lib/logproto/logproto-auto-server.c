/*
 * Copyright (c) 2024 Balázs Scheidler <balazs.scheidler@axoflow.com>
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
#include "logproto-auto-server.h"
#include "logproto-text-server.h"
#include "logproto-framed-server.h"
#include "messages.h"

typedef struct _LogProtoAutoServer
{
  LogProtoServer super;

  /* the actual LogProto instance that we run after auto-detecting the protocol */
  LogProtoServer *proto_impl;
} LogProtoAutoServer;

static LogProtoServer *
_construct_detected_proto(LogProtoAutoServer *self, const gchar *detect_buffer, gsize detect_buffer_len)
{
  if (g_ascii_isdigit(detect_buffer[0]))
    {
      msg_debug("Auto-detected octet-counted-framing on RFC6587 connection, using framed transport",
                evt_tag_int("fd", self->super.transport->fd));
      return log_proto_framed_server_new(self->super.transport, self->super.options);
    }
  if (detect_buffer[0] == '<')
    {
      msg_debug("Auto-detected non-transparent-framing on RFC6587 connection, using simple text transport",
                evt_tag_int("fd", self->super.transport->fd));
    }
  else
    {
      msg_debug("Unable to detect framing on RFC6587 connection, falling back to simple text transport",
                evt_tag_int("fd", self->super.transport->fd),
                evt_tag_mem("detect_buffer", detect_buffer, detect_buffer_len));
    }
  return log_proto_text_server_new(self->super.transport, self->super.options);
}

static LogProtoPrepareAction
log_proto_auto_server_prepare(LogProtoServer *s, GIOCondition *cond, gint *timeout G_GNUC_UNUSED)
{
  LogProtoAutoServer *self = (LogProtoAutoServer *) s;

  if (self->proto_impl)
    return log_proto_server_prepare(self->proto_impl, cond, timeout);

  *cond = self->super.transport->cond;
  if (*cond == 0)
    *cond = G_IO_IN;

  return LPPA_POLL_IO;
}

static LogProtoStatus
log_proto_auto_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                            LogTransportAuxData *aux, Bookmark *bookmark)
{
  LogProtoAutoServer *self = (LogProtoAutoServer *) s;

  if (self->proto_impl)
    return log_proto_server_fetch(self->proto_impl, msg, msg_len, may_read, aux, bookmark);

  g_assert_not_reached();
}

static LogProtoStatus
log_proto_auto_handshake(LogProtoServer *s, gboolean *handshake_finished)
{
  LogProtoAutoServer *self = (LogProtoAutoServer *) s;
  /* allow the impl to do its handshake */
  if (self->proto_impl)
    return log_proto_server_handshake(self->proto_impl, handshake_finished);

  gchar detect_buffer[8];
  gint rc;

  rc = log_transport_read_ahead(self->super.transport, detect_buffer, sizeof(detect_buffer));
  if (rc == 0)
    return LPS_EOF;
  else if (rc < 0)
    return LPS_ERROR;

  self->proto_impl = _construct_detected_proto(self, detect_buffer, rc);
  if (self->proto_impl)
    {
      /* transport is handed over to the new proto */
      self->super.transport = NULL;
      return LPS_SUCCESS;
    }
  return LPS_ERROR;
}

static void
log_proto_auto_server_free(LogProtoServer *s)
{
  LogProtoAutoServer *self = (LogProtoAutoServer *) s;

  if (self->proto_impl)
    log_proto_server_free(self->proto_impl);
  log_proto_server_free_method(s);
}

LogProtoServer *
log_proto_auto_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoAutoServer *self = g_new0(LogProtoAutoServer, 1);

  log_proto_server_init(&self->super, transport, options);
  self->super.handshake = log_proto_auto_handshake;
  self->super.prepare = log_proto_auto_server_prepare;
  self->super.fetch = log_proto_auto_server_fetch;
  self->super.free_fn = log_proto_auto_server_free;
  return &self->super;
}
