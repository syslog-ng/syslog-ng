/*
 * Copyright (c) 2023 One Identity LLC.
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

#include "logproto-framed-proxied-text-server.h"
#include "logproto-proxied-text-server.h"
#include "logproto-proxied-text-server-private.h"
#include "logproto-framed-server-private.h"

typedef struct _LogProtoFramedProxiedTextServer
{
  LogProtoProxiedTextServer super;
  LogProtoFramedServer *framed_server;
} LogProtoFramedProxiedTextServer;

static LogProtoStatus
log_proto_framed_proxied_text_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                                           LogTransportAuxData *aux, Bookmark *bookmark)
{
  LogProtoFramedProxiedTextServer *self = (LogProtoFramedProxiedTextServer *) s;

  LogProtoStatus status;

  if (!self->framed_server->buffer)
    {
      // read data using the proxied server
      log_proto_proxied_text_server_fetch((LogProtoServer *)&self->super, msg, msg_len, may_read, aux, bookmark);

      // set up state so that the framed server
      // is already aware that data has been read
      self->framed_server->state = LPFSS_FRAME_EXTRACT;
      self->framed_server->buffer = (guchar *)g_strdup((const gchar *)*msg);
      self->framed_server->buffer_pos = 0;
      self->framed_server->buffer_end = (guint32)strlen((const char *)self->framed_server->buffer);
      self->framed_server->frame_len = 0;
    }

  self->framed_server->fetch_counter = 0;
  while (_step_state_machine(self->framed_server, msg, msg_len, may_read, aux, &status) != LPFSSCTRL_RETURN_WITH_STATUS) ;

  return status;
}

static void
log_proto_framed_proxied_text_server_free(LogProtoServer *s)
{
  LogProtoFramedProxiedTextServer *self = (LogProtoFramedProxiedTextServer *) s;

  self->framed_server->super.free_fn((LogProtoServer *)self->framed_server);
  self->framed_server = NULL;
  log_proto_proxied_text_server_free((LogProtoServer *)&self->super);
}

LogProtoServer *
log_proto_framed_proxied_text_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoFramedProxiedTextServer *self = g_new0(LogProtoFramedProxiedTextServer, 1);

  log_proto_proxied_text_server_init(&self->super, transport, options);
  self->framed_server = (LogProtoFramedServer *)log_proto_framed_server_new(NULL, options);

  self->super.super.super.super.fetch = log_proto_framed_proxied_text_server_fetch;
  self->super.super.super.super.free_fn = log_proto_framed_proxied_text_server_free;

  return &self->super.super.super.super;
}

LogProtoServer *log_proto_framed_proxied_text_tls_passthrough_server_new(LogTransport *transport,
    const LogProtoServerOptions *options)
{
  LogProtoFramedProxiedTextServer *self = (LogProtoFramedProxiedTextServer *)
                                          log_proto_framed_proxied_text_server_new(transport, options);
  self->super.has_to_switch_to_tls = TRUE;
  return &self->super.super.super.super;
}
