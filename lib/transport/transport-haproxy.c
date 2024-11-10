/*
 * Copyright (c) 2020-2023 One Identity LLC.
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "transport/transport-haproxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "messages.h"
#include "str-utils.h"

#define IP_BUF_SIZE 64

/* This class implements: https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt */

/* the size of the buffer we use to fetch the PROXY header into */
#define PROXY_PROTO_HDR_BUFFER_SIZE 1500

typedef struct _LogTransportHAProxy LogTransportHAProxy;
struct _LogTransportHAProxy
{
  LogTransportAdapter super;
  LogTransportIndex switch_to;

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



/* This class implements: https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt */

/* PROXYv1 line without newlines or terminating zero character.  The
 * protocol specification contains the number 108 that includes both the
 * CRLF sequence and the NUL */
#define PROXY_PROTO_HDR_MAX_LEN_RFC 105

/* the amount of bytes we need from the client to detect protocol version */
#define PROXY_PROTO_HDR_MAGIC_LEN   5

typedef enum
{
  STATUS_SUCCESS,
  STATUS_ERROR,
  STATUS_EOF,
  STATUS_AGAIN,
} Status;

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

#define PROXY_HDR_TCP4 "PROXY TCP4 "
#define PROXY_HDR_TCP6 "PROXY TCP6 "
#define PROXY_HDR_UNKNOWN "PROXY UNKNOWN"

static gboolean
_check_proxy_v1_header_length(const guchar *msg, gsize msg_len)
{
  if (msg_len > PROXY_PROTO_HDR_MAX_LEN_RFC)
    {
      msg_error("PROXY proto header length exceeds max length defined by the specification",
                evt_tag_long("length", msg_len),
                evt_tag_str("header", (const gchar *)msg));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_check_proxy_v1_header(const guchar *msg, gsize msg_len, const gchar *expected_header, gsize *header_len)
{
  gsize expected_header_length = strlen(expected_header);

  if (msg_len < expected_header_length)
    return FALSE;

  *header_len = expected_header_length;
  return strncmp((const gchar *)msg, expected_header, expected_header_length) == 0;
}

static gboolean
_is_proxy_v1_proto_tcp4(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_proxy_v1_header(msg, msg_len, PROXY_HDR_TCP4, header_len);
}

static gboolean
_is_proxy_v1_proto_tcp6(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_proxy_v1_header(msg, msg_len, PROXY_HDR_TCP6, header_len);
}

static gboolean
_is_proxy_v1_unknown(const guchar *msg, gsize msg_len, gsize *header_len)
{
  return _check_proxy_v1_header(msg, msg_len, PROXY_HDR_UNKNOWN, header_len);
}

static gboolean
_parse_proxy_v1_unknown_header(LogTransportHAProxy *self, const guchar *msg, gsize msg_len)
{
  if (msg_len == 0)
    return TRUE;

  msg_warning("PROXY UNKNOWN header contains unexpected parameters",
              evt_tag_mem("parameters", msg, msg_len));

  return TRUE;
}

static gboolean
_parse_proxy_v1_tcp_header(LogTransportHAProxy *self, const guchar *msg, gsize msg_len)
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

  g_strlcpy(self->info.src_ip, params[0], IP_BUF_SIZE);
  g_strlcpy(self->info.dst_ip, params[1], IP_BUF_SIZE);

  self->info.src_port = atoi(params[2]);
  if (self->info.src_port > 65535 || self->info.src_port < 0)
    msg_warning("PROXT TCP header contains invalid src port", evt_tag_str("src port", params[2]));

  self->info.dst_port = atoi(params[3]);
  if (self->info.dst_port > 65535 || self->info.dst_port < 0)
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
_extract_proxy_v1_header(LogTransportHAProxy *self, guchar **msg, gsize *msg_len)
{
  if (self->proxy_header_buff[self->proxy_header_buff_len - 1] == '\n')
    self->proxy_header_buff_len--;

  if (self->proxy_header_buff[self->proxy_header_buff_len - 1] == '\r')
    self->proxy_header_buff_len--;

  self->proxy_header_buff[self->proxy_header_buff_len] = 0;
  *msg = self->proxy_header_buff;
  *msg_len = self->proxy_header_buff_len;
  return TRUE;
}

static gboolean
_parse_proxy_v1_header(LogTransportHAProxy *self)
{
  guchar *proxy_line;
  gsize proxy_line_len;

  if (!_extract_proxy_v1_header(self, &proxy_line, &proxy_line_len))
    return FALSE;


  gsize header_len = 0;

  if (!_check_proxy_v1_header_length(proxy_line, proxy_line_len))
    return FALSE;

  if (_is_proxy_v1_unknown(proxy_line, proxy_line_len, &header_len))
    {
      self->info.unknown = TRUE;
      return _parse_proxy_v1_unknown_header(self, proxy_line + header_len, proxy_line_len - header_len);
    }

  if (_is_proxy_v1_proto_tcp4(proxy_line, proxy_line_len, &header_len))
    {
      self->info.ip_version = 4;
      return _parse_proxy_v1_tcp_header(self, proxy_line + header_len, proxy_line_len - header_len);
    }

  if (_is_proxy_v1_proto_tcp6(proxy_line, proxy_line_len, &header_len))
    {
      self->info.ip_version = 6;
      return _parse_proxy_v1_tcp_header(self, proxy_line + header_len, proxy_line_len - header_len);
    }

  return FALSE;
}

static gboolean
_parse_proxy_v2_proxy_address(LogTransportHAProxy *self, struct proxy_hdr_v2 *proxy_hdr,
                              union proxy_addr *proxy_addr)
{
  gint address_family = (proxy_hdr->fam & 0xF0) >> 4;
  gint proxy_header_len = ntohs(proxy_hdr->len);

  if (address_family == 1 && proxy_header_len >= sizeof(proxy_addr->ipv4_addr))
    {
      /* AF_INET */
      inet_ntop(AF_INET, (gchar *) &proxy_addr->ipv4_addr.src_addr, self->info.src_ip, sizeof(self->info.src_ip));
      inet_ntop(AF_INET, (gchar *) &proxy_addr->ipv4_addr.dst_addr, self->info.dst_ip, sizeof(self->info.dst_ip));
      self->info.src_port = ntohs(proxy_addr->ipv4_addr.src_port);
      self->info.dst_port = ntohs(proxy_addr->ipv4_addr.dst_port);
      self->info.ip_version = 4;
      return TRUE;
    }
  else if (address_family == 2 && proxy_header_len >= sizeof(proxy_addr->ipv6_addr))
    {
      /* AF_INET6 */
      inet_ntop(AF_INET6, (gchar *) &proxy_addr->ipv6_addr.src_addr, self->info.src_ip, sizeof(self->info.src_ip));
      inet_ntop(AF_INET6, (gchar *) &proxy_addr->ipv6_addr.dst_addr, self->info.dst_ip, sizeof(self->info.dst_ip));
      self->info.src_port = ntohs(proxy_addr->ipv6_addr.src_port);
      self->info.dst_port = ntohs(proxy_addr->ipv6_addr.dst_port);
      self->info.ip_version = 6;
      return TRUE;
    }
  else if (address_family == 0)
    {
      /* AF_UNSPEC */
      self->info.unknown = TRUE;
      return TRUE;
    }

  msg_error("PROXYv2 header does not have enough bytes to represent endpoint addresses or unknown address_family",
            evt_tag_int("proxy_header_len", proxy_header_len),
            evt_tag_int("address_family", address_family));
  return FALSE;
}

static gboolean
_parse_proxy_v2_header(LogTransportHAProxy *self)
{
  struct proxy_hdr_v2 *proxy_hdr = (struct proxy_hdr_v2 *) self->proxy_header_buff;
  union proxy_addr *proxy_addr = (union proxy_addr *)(proxy_hdr + 1);

  /* is this proxy v2 */
  if ((proxy_hdr->ver_cmd & 0xF0) != 0x20)
    return FALSE;

  if ((proxy_hdr->ver_cmd & 0xF) == 0)
    {
      /* LOCAL connection */
      return TRUE;
    }
  else if ((proxy_hdr->ver_cmd & 0xF) == 1)
    {
      /* PROXY connection */
      return _parse_proxy_v2_proxy_address(self, proxy_hdr, proxy_addr);
    }

  return FALSE;
}

gboolean
_parse_proxy_header(LogTransportHAProxy *self)
{
  if (self->proxy_header_version == 1)
    return _parse_proxy_v1_header(self);
  else if (self->proxy_header_version == 2)
    return _parse_proxy_v2_header(self);
  else
    g_assert_not_reached();
}

static Status
_fetch_chunk(LogTransportHAProxy *self, gsize upto_bytes)
{
  g_assert(upto_bytes < sizeof(self->proxy_header_buff));
  if (self->proxy_header_buff_len < upto_bytes)
    {
      gssize rc = log_transport_adapter_read_method(&self->super.super,
                                                    &(self->proxy_header_buff[self->proxy_header_buff_len]),
                                                    upto_bytes - self->proxy_header_buff_len, NULL);
      if (rc < 0)
        {
          if (errno == EAGAIN)
            return STATUS_AGAIN;
          else
            {
              msg_error("I/O error occurred while reading proxy header",
                        evt_tag_error(EVT_TAG_OSERROR));
              return STATUS_ERROR;
            }
        }
      /* EOF without data */
      if (rc == 0)
        {
          return STATUS_EOF;
        }

      self->proxy_header_buff_len += rc;
    }
  if (self->proxy_header_buff_len == upto_bytes)
    return STATUS_SUCCESS;
  return STATUS_AGAIN;
}

static inline Status
_fetch_until_newline(LogTransportHAProxy *self)
{
  /* leave 1 character for terminating zero.  We should have plenty of space
   * in our buffer, as the longest line we need to fetch is 107 bytes
   * including the line terminator characters. */

  while (self->proxy_header_buff_len < sizeof(self->proxy_header_buff) - 1)
    {
      Status status = _fetch_chunk(self, self->proxy_header_buff_len + 1);

      if (status != STATUS_SUCCESS)
        return status;

      if (self->proxy_header_buff[self->proxy_header_buff_len - 1] == '\n')
        {
          return STATUS_SUCCESS;
        }
    }

  msg_error("PROXY proto header with invalid header length",
            evt_tag_int("max_parsable_length", sizeof(self->proxy_header_buff)-1),
            evt_tag_int("max_length_by_spec", PROXY_PROTO_HDR_MAX_LEN_RFC),
            evt_tag_long("length", self->proxy_header_buff_len),
            evt_tag_str("header", (const gchar *)self->proxy_header_buff));
  return STATUS_ERROR;
}

static Status
_fetch_proxy_v2_payload(LogTransportHAProxy *self)
{
  struct proxy_hdr_v2 *hdr = (struct proxy_hdr_v2 *) self->proxy_header_buff;
  gsize proxy_header_len = sizeof(*hdr) + ntohs(hdr->len);

  if (proxy_header_len > sizeof(self->proxy_header_buff))
    {
      msg_error("PROXYv2 proto header with invalid header length",
                evt_tag_int("max_parsable_length", sizeof(self->proxy_header_buff)),
                evt_tag_long("length", proxy_header_len));
      return STATUS_ERROR;
    }

  return _fetch_chunk(self, proxy_header_len);
}

static gboolean
_is_proxy_version_v1(LogTransportHAProxy *self)
{
  if (self->proxy_header_buff_len < PROXY_PROTO_HDR_MAGIC_LEN)
    return FALSE;

  return memcmp(self->proxy_header_buff, "PROXY", PROXY_PROTO_HDR_MAGIC_LEN) == 0;
}

static gboolean
_is_proxy_version_v2(LogTransportHAProxy *self)
{
  if (self->proxy_header_buff_len < PROXY_PROTO_HDR_MAGIC_LEN)
    return FALSE;

  return memcmp(self->proxy_header_buff, "\x0D\x0A\x0D\x0A\x00", PROXY_PROTO_HDR_MAGIC_LEN) == 0;
}

static inline Status
_fetch_into_proxy_buffer(LogTransportHAProxy *self)
{
  Status status;

  switch (self->header_fetch_state)
    {
    case LPPTS_INITIAL:
      self->header_fetch_state = LPPTS_DETERMINE_VERSION;
    /* fallthrough */
    case LPPTS_DETERMINE_VERSION:
      status = _fetch_chunk(self, PROXY_PROTO_HDR_MAGIC_LEN);

      if (status != STATUS_SUCCESS)
        return status;

      if (_is_proxy_version_v1(self))
        {
          self->header_fetch_state = LPPTS_PROXY_V1_READ_LINE;
          self->proxy_header_version = 1;
          goto process_proxy_v1;
        }
      else if (_is_proxy_version_v2(self))
        {
          self->header_fetch_state = LPPTS_PROXY_V2_READ_HEADER;
          self->proxy_header_version = 2;
          goto process_proxy_v2;
        }
      else
        {
          msg_error("Unable to determine PROXY protocol version");
          return STATUS_ERROR;
        }
      g_assert_not_reached();

    case LPPTS_PROXY_V1_READ_LINE:

process_proxy_v1:
      return _fetch_until_newline(self);
    case LPPTS_PROXY_V2_READ_HEADER:
process_proxy_v2:
      status = _fetch_chunk(self, sizeof(struct proxy_hdr_v2));
      if (status != STATUS_SUCCESS)
        return status;

      self->header_fetch_state = LPPTS_PROXY_V2_READ_PAYLOAD;
    /* fallthrough */
    case LPPTS_PROXY_V2_READ_PAYLOAD:
      return _fetch_proxy_v2_payload(self);
    default:
      g_assert_not_reached();
    }
}

static void
_save_addresses(LogTransportHAProxy *self)
{
  LogTransportStack *stack = self->super.super.stack;
  if (self->info.unknown)
    return;

  if (self->info.ip_version == 4)
    {
      log_transport_aux_data_set_peer_addr_ref(&stack->aux_data,
                                               g_sockaddr_inet_new(self->info.src_ip, self->info.src_port));
      log_transport_aux_data_set_local_addr_ref(&stack->aux_data,
                                                g_sockaddr_inet_new(self->info.dst_ip, self->info.dst_port));
    }
  else if (self->info.ip_version == 6)
    {
      log_transport_aux_data_set_peer_addr_ref(&stack->aux_data,
                                               g_sockaddr_inet6_new(self->info.src_ip, self->info.src_port));
      log_transport_aux_data_set_local_addr_ref(&stack->aux_data,
                                                g_sockaddr_inet6_new(self->info.dst_ip, self->info.dst_port));
    }
  else
    g_assert_not_reached();
}

static Status
_proccess_proxy_header(LogTransportHAProxy *self)
{
  Status status = _fetch_into_proxy_buffer(self);

  self->proxy_header_processed = (status == STATUS_SUCCESS);
  if (status != STATUS_SUCCESS)
    return status;

  gboolean parsable = _parse_proxy_header(self);

  msg_debug("PROXY protocol header received",
            evt_tag_int("version", self->proxy_header_version),
            self->proxy_header_version == 1
            ? evt_tag_mem("header", self->proxy_header_buff, self->proxy_header_buff_len)
            : evt_tag_str("header", "<binary_data>"));

  if (parsable)
    {
      msg_trace("PROXY protocol header parsed successfully");
      _save_addresses(self);

      return STATUS_SUCCESS;
    }
  else
    {
      msg_error("Error parsing PROXY protocol header");
      return STATUS_ERROR;
    }
}

static gssize
_haproxy_read(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportHAProxy *self = (LogTransportHAProxy *)s;

  if (!self->proxy_header_processed)
    {
      Status status = _proccess_proxy_header(self);
      if (status != STATUS_SUCCESS)
        {
          if (status == STATUS_ERROR || status == STATUS_EOF)
            errno = EINVAL;
          else if (status == STATUS_AGAIN)
            errno = EAGAIN;
          return -1;
        }

      if (!log_transport_stack_switch(self->super.super.stack, self->switch_to))
        g_assert_not_reached();

      errno = EAGAIN;
      return -1;
    }
  g_assert_not_reached();
}

LogTransport *
log_transport_haproxy_new(LogTransportIndex base, LogTransportIndex switch_to)
{
  LogTransportHAProxy *self = g_new0(LogTransportHAProxy, 1);

  log_transport_adapter_init_instance(&self->super, "haproxy", base);
  self->super.super.read = _haproxy_read;
  self->switch_to = switch_to;

  return &self->super.super;
}
