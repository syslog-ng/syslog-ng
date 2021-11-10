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

#ifndef LOGPROTO_PROXIED_TEXT_SERVER
#define LOGPROTO_PROXIED_TEXT_SERVER

#include "logproto-text-server.h"

#define IP_BUF_SIZE 64
#define PROXY_PROTO_HDR_MAX_LEN_RFC 108
#define PROXY_PROTO_HDR_MAX_LEN (PROXY_PROTO_HDR_MAX_LEN_RFC * 2)

struct ProxyProtoInfo
{
  gboolean unknown;

  gchar src_ip[IP_BUF_SIZE];
  gchar dst_ip[IP_BUF_SIZE];

  int ip_version;
  int src_port;
  int dst_port;
};

typedef struct _LogProtoProxiedTextServer
{
  LogProtoTextServer super;

  // Info received from the proxied that should be added as LogTransportAuxData to
  // any message received through this server instance.
  struct ProxyProtoInfo *info;

  // Flag to only run handshake() once
  gboolean handshake_done;
  gboolean has_to_switch_to_tls;

  guchar proxy_header_buff[PROXY_PROTO_HDR_MAX_LEN + 1];
  gsize proxy_header_buff_len;
} LogProtoProxiedTextServer;

LogProtoServer *log_proto_proxied_text_server_new(LogTransport *transport, const LogProtoServerOptions *options);
LogProtoServer *log_proto_proxied_text_tls_passthrough_server_new(LogTransport *transport,
    const LogProtoServerOptions *options);

#endif
