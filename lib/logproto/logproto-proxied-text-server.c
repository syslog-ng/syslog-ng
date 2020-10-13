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
#include <stdlib.h>
#include "messages.h"
#include "logproto-proxied-text-server.h"
#include "str-utils.h"

#define PROXY_HDR_TCP4 "PROXY TCP4 "
#define PROXY_HDR_TCP6 "PROXY TCP6 "
#define PROXY_HDR_UNKNOWN "PROXY UNKNOWN"
#define PROXY_PROTO_HDR_MAX_LEN_RFC 108
#define PROXY_PROTO_HDR_MAX_LEN (PROXY_PROTO_HDR_MAX_LEN_RFC * 2)

static gboolean
_check_header_length(const guchar *msg, gsize msg_len)
{
  if (msg_len > PROXY_PROTO_HDR_MAX_LEN_RFC)
    {
      msg_warning("PROXY proto header length exceeds max. length defined by specification",
                  evt_tag_long("length", msg_len),
                  evt_tag_str("header", (const gchar *)msg));
    }

  if (msg_len > PROXY_PROTO_HDR_MAX_LEN)
    {
      msg_error("PROXY proto header with invalid header length",
                evt_tag_int("max_parsable_length", PROXY_PROTO_HDR_MAX_LEN),
                evt_tag_int("max_length_by_spec", PROXY_PROTO_HDR_MAX_LEN_RFC),
                evt_tag_long("length", msg_len),
                evt_tag_str("header", (const gchar *)msg));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_check_header(const guchar *msg, gsize msg_len, const gchar *expected_header, gsize *header_len)
{
  gsize expected_header_length = strlen(expected_header);

  if (msg_len < expected_header_length)
    return FALSE;

  *header_len = expected_header_length;
  return strncmp((const gchar *)msg, expected_header, expected_header_length) == 0;
}

static gboolean
_is_proxy_proto_tcp4(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_header(msg, msg_len, PROXY_HDR_TCP4, header_len);
}

static gboolean
_is_proxy_proto_tcp6(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_header(msg, msg_len, PROXY_HDR_TCP6, header_len);
}

static gboolean
_is_proxy_unknown(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_header(msg, msg_len, PROXY_HDR_UNKNOWN, header_len);
}

static gboolean
_parse_unknown_header(LogProtoProxiedTextServer *self, const guchar *msg, gsize msg_len)
{
  if (msg_len == 0)
    return TRUE;

  msg_warning("PROXY UNKNOWN header contains unexpected paramters",
              evt_tag_printf("parameters", "%.*s", (gint) msg_len, msg));

  return TRUE;
}

static gboolean
_parse_tcp_header(LogProtoProxiedTextServer *self, const guchar *msg, gsize msg_len)
{
  if (msg_len == 0)
    return FALSE;

  GString *params_str = g_string_new_len((const gchar *)msg, msg_len);
  gboolean result = FALSE;
  msg_debug("PROXY header params", evt_tag_str("params", (const gchar *)msg));

  gchar **params = strsplit(params_str->str, ' ', 5);
  gint params_n = g_strv_length(params);
  if (params_n < 4)
    goto ret;

  strncpy(self->info->src_ip, params[0], IP_BUF_SIZE - 1);
  strncpy(self->info->dst_ip, params[1], IP_BUF_SIZE - 1);

  self->info->src_port = atoi(params[2]);
  if (self->info->src_port > 65535 || self->info->src_port < 0)
    msg_warning("PROXT TCP header contains invalid src port", evt_tag_str("src port", params[2]));

  self->info->dst_port = atoi(params[3]);
  if (self->info->dst_port > 65535 || self->info->dst_port < 0)
    msg_warning("PROXT TCP header contains invalid dst port", evt_tag_str("dst port", params[2]));

  if (params_n > 4)
    msg_warning("PROXY TCP header contains unexpected paramaters", evt_tag_str("parameters", params[4]));

  result = TRUE;
ret:
  if (params)
    g_strfreev(params);
  g_string_free(params_str, TRUE);

  return result;
}

static gboolean
_log_proto_proxied_text_server_parse_header(LogProtoProxiedTextServer *self, const guchar *msg, gsize msg_len)
{
  gsize header_len = 0;

  if (!_check_header_length(msg, msg_len))
    return FALSE;

  if (_is_proxy_unknown(msg, msg_len, &header_len))
    {
      self->info->unknown = TRUE;
      return _parse_unknown_header(self, msg + header_len, msg_len - header_len);
    }

  if (_is_proxy_proto_tcp4(msg, msg_len, &header_len))
    {
      self->info->ip_version = 4;
      return _parse_tcp_header(self, msg + header_len, msg_len - header_len);
    }

  if (_is_proxy_proto_tcp6(msg, msg_len, &header_len))
    {
      self->info->ip_version = 6;
      return _parse_tcp_header(self, msg + header_len, msg_len - header_len);
    }

  return FALSE;
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
