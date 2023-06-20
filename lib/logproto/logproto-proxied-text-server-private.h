/*
 * Copyright (c) 2023 One Identity
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

#ifndef LOGPROTO_PROXIED_TEXT_SERVER_PRIVATE_H_INCLUDED
#define LOGPROTO_PROXIED_TEXT_SERVER_PRIVATE_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#include "messages.h"
#include "logproto-proxied-text-server.h"
#include "transport/multitransport.h"
#include "transport/transport-factory-tls.h"
#include "str-utils.h"

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

typedef struct _LogProtoProxiedTextServer
{
  LogProtoTextServer super;

  // Info received from the proxied that should be added as LogTransportAuxData to
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

  // Flag to only run handshake() once
  gboolean handshake_done;
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
} LogProtoProxiedTextServer;


struct proxy_hdr_v2
{
  uint8_t sig[12];  /* hex 0D 0A 0D 0A 00 0D 0A 51 55 49 54 0A */
  uint8_t ver_cmd;  /* protocol version and command */
  uint8_t fam;      /* protocol family and address */
  uint16_t len;     /* number of following bytes part of the header */
};

union proxy_addr
{
  struct
  {
    /* for TCP/UDP over IPv4, len = 12 */
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
  } ipv4_addr;
  struct
  {
    /* for TCP/UDP over IPv6, len = 36 */
    uint8_t  src_addr[16];
    uint8_t  dst_addr[16];
    uint16_t src_port;
    uint16_t dst_port;
  } ipv6_addr;
  struct
  {
    /* for AF_UNIX sockets, len = 216 */
    uint8_t src_addr[108];
    uint8_t dst_addr[108];
  } unix_addr;
};

LogProtoStatus
log_proto_proxied_text_server_fetch(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                                    LogTransportAuxData *aux, Bookmark *bookmark);
void
log_proto_proxied_text_server_free(LogProtoServer *s);

void
log_proto_proxied_text_server_init(LogProtoProxiedTextServer *self, LogTransport *transport,
                                   const LogProtoServerOptions *options);

#endif
