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

#include "logtransport.h"

#define IP_BUF_SIZE 64

/* This class implements: https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt */

/* the size of the buffer we use to fetch the PROXY header into */
#define PROXY_PROTO_HDR_BUFFER_SIZE 1500

typedef struct _LogTransportSocketProxy LogTransportSocketProxy;
struct _LogTransportSocketProxy
{
  LogTransport *base_transport;
  gssize (*base_read)(LogTransport *self, gpointer buf, gsize count, LogTransportAuxData *aux);
  gssize (*base_write)(LogTransport *self, const gpointer buf, gsize count);
  gboolean should_switch_transport;

  /* Info received from the proxy that should be added as LogTransportAuxData to
   * any message received through this server instance. */
  struct
  {
    gboolean unknown;

    gchar src_ip[IP_BUF_SIZE];
    gchar dst_ip[IP_BUF_SIZE];

    int ip_version;
    int src_port;
    int dst_port;
  } info;

  /* Flag to only process proxy header once */
  gboolean proxy_header_processed;

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

LogTransportSocketProxy *log_transport_socket_proxy_new(LogTransport *base_transport, gboolean is_multi);
void log_transport_socket_proxy_free(LogTransportSocketProxy *self);

#endif
