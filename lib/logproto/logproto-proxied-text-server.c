/*
 * Copyright (c) 2020 One Identity
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

#include <stdio.h>

#include "messages.h"
#include "logproto-proxied-text-server.h"

#define PROXY_TCP_HDR "PROXY TCP%d %s %s %d %d"
#define PROXY_UNKNOWN_HDR "PROXY UNKNOWN"

gboolean
_log_proto_proxied_text_server_parse_header(LogProtoProxiedTextServer *self, const guchar *msg, gsize msg_len)
{
  GString *input = g_string_new_len((const gchar *)msg, msg_len);
  int matches = 0;
  int compares = -1;

  matches = sscanf(input->str, PROXY_TCP_HDR, &self->info->ip_version, self->info->src_ip, self->info->dst_ip,
                   &self->info->src_port, &self->info->dst_port);

  if (matches != 5)
    compares = strcmp(input->str, PROXY_UNKNOWN_HDR);

  g_string_free(input, TRUE);

  if (compares == 0)
    {
      msg_info("PROXY UNKOWN valid protocol header received");
      self->info->unknown = TRUE;
    }

  return (matches == 5 || compares == 0) ? TRUE : FALSE;
}

static void
_log_proto_proxied_text_server_add_aux_data(LogProtoProxiedTextServer *self, LogTransportAuxData *aux)
{
  gchar buf1[8];
  gchar buf2[8];
  gchar buf3[8];

  if (self->info->unknown)
    return;

  snprintf(buf1, 8, "%i", self->info->src_port);
  snprintf(buf2, 8, "%i", self->info->dst_port);
  snprintf(buf3, 8, "%i", self->info->ip_version);

  log_transport_aux_data_add_nv_pair(aux, "PROXIED_SRCIP", self->info->src_ip);
  log_transport_aux_data_add_nv_pair(aux, "PROXIED_DSTIP", self->info->dst_ip);
  log_transport_aux_data_add_nv_pair(aux, "PROXIED_SRCPORT", buf1);
  log_transport_aux_data_add_nv_pair(aux, "PROXIED_DSTPORT", buf2);
  log_transport_aux_data_add_nv_pair(aux, "PROXIED_IP_VERSION", buf3);

  return;
}

static LogProtoStatus
_log_proto_proxied_text_server_handshake(LogProtoServer *s)
{
  const guchar *msg;
  gsize msg_len;
  gboolean may_read = TRUE;
  gboolean parsable;
  LogProtoStatus status;

  LogProtoProxiedTextServer *self = (LogProtoProxiedTextServer *) s;

  // Fetch a line from the transport layer
  status = self->fetch_from_buffer(&self->super.super.super, &msg, &msg_len, &may_read, NULL, NULL);

  if (status != LPS_SUCCESS)
    {
      if (status == LPS_AGAIN)
        self->handshake_done = FALSE;
      return status;
    }

  parsable = _log_proto_proxied_text_server_parse_header(self, msg, msg_len);

  msg_debug("PROXY protocol header received", evt_tag_printf("line", "%.*s", (gint) msg_len, msg));

  if (parsable)
    {
      msg_info("PROXY protocol header parsed successfully");
      return LPS_SUCCESS;
    }
  else
    {
      msg_error("Error parsing PROXY protocol header");
      return LPS_ERROR;
    }
}

static gboolean
_log_proto_proxied_text_server_handshake_in_progress(LogProtoServer *s)
{
  LogProtoProxiedTextServer *self = (LogProtoProxiedTextServer *) s;

  // Run handshake() only once
  if (!self->handshake_done)
    {
      self->handshake_done = TRUE;
      return TRUE;
    }
  return FALSE;
}

static LogProtoStatus
_log_proto_proxied_text_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                                     LogTransportAuxData *aux, Bookmark *bookmark)
{
  LogProtoProxiedTextServer *self = (LogProtoProxiedTextServer *) s;

  LogProtoStatus status = self->fetch_from_buffer(&self->super.super.super, msg, msg_len, may_read, aux, bookmark);

  if (status != LPS_SUCCESS)
    return status;

  _log_proto_proxied_text_server_add_aux_data(self, aux);

  return LPS_SUCCESS;
}

static void
_log_proto_proxied_text_server_free(LogProtoServer *s)
{
  LogProtoProxiedTextServer *self = (LogProtoProxiedTextServer *) s;

  msg_debug("Freeing PROXY protocol source driver", evt_tag_printf("driver", "%p", self));

  g_free((gpointer) self->info);

  log_proto_text_server_free(&self->super.super.super);

  return;
}

static void
_log_proto_proxied_text_server_init(LogProtoProxiedTextServer *self, LogTransport *transport,
                                    const LogProtoServerOptions *options)
{
  msg_info("Initializing PROXY protocol source driver", evt_tag_printf("driver", "%p", self));

  log_proto_text_server_init(&self->super, transport, options);

  self->fetch_from_buffer = self->super.super.super.fetch;

  self->super.super.super.fetch = _log_proto_proxied_text_server_fetch;
  self->super.super.super.free_fn = _log_proto_proxied_text_server_free;
  self->super.super.super.handshake_in_progess = _log_proto_proxied_text_server_handshake_in_progress;
  self->super.super.super.handshake = _log_proto_proxied_text_server_handshake;

  return;
}


LogProtoServer *
log_proto_proxied_text_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoProxiedTextServer *self = g_new0(LogProtoProxiedTextServer, 1);
  self->info = g_new0(struct ProxyProtoInfo, 1);

  _log_proto_proxied_text_server_init(self, transport, options);

  return &self->super.super.super;
}
