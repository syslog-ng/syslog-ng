/*
 * Copyright (c) 2020-2023 One Identity LLC.
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

#ifndef TRANSPORT_SOCKET_PROXY_H_INCLUDED
#define TRANSPORT_SOCKET_PROXY_H_INCLUDED

#include "transport-socket.h"
#include "multitransport.h"

#define IP_BUF_SIZE 64

/* This class implements: https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt */

/* PROXYv1 line without newlines or terminating zero character.  The
 * protocol specification contains the number 108 that includes both the
 * CRLF sequence and the NUL */
#define PROXY_PROTO_HDR_MAX_LEN_RFC 105

/* the size of the buffer we use to fetch the PROXY header into */
#define PROXY_PROTO_HDR_BUFFER_SIZE 1500

/* the amount of bytes we need from the client to detect protocol version */
#define PROXY_PROTO_HDR_MAGIC_LEN   5

typedef struct _LogTransportSocketProxy LogTransportSocketProxy;
struct _LogTransportSocketProxy
{
  MultiTransport super;

  // Info received from the proxy that should be added as LogTransportAuxData to
  // any message received through this server instance.
  struct
  {
    gboolean unknown;

    gchar src_ip[IP_BUF_SIZE];
    gchar dst_ip[IP_BUF_SIZE];

    int ip_version;
    int src_port;
    int dst_port;
  } info;

  // Flag to only process proxy header once
  gboolean proxy_header_processed;

  gboolean has_to_switch_to_tls;

  enum
  {
    LPPTS_INITIAL,
    LPPTS_DETERMINE_VERSION,
    LPPTS_PROXY_V1_READ_LINE,
    LPPTS_PROXY_V2_READ_HEADER,
    LPPTS_PROXY_V2_READ_PAYLOAD,
  } header_fetch_state;

  /* 0 unknown, 1 or 2 indicate proxy header version */
  gint proxy_header_version;
  guchar proxy_header_buff[PROXY_PROTO_HDR_BUFFER_SIZE];
  gsize proxy_header_buff_len;
};

LogTransport *log_transport_proxied_stream_socket_new(gint fd);
LogTransport *log_transport_proxied_stream_socket_with_tls_passthrough_new(gint fd);

#endif
