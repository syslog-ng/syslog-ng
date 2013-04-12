/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "transport-mapper-inet.h"
#include "afinet.h"
#include "cfg.h"
#include "messages.h"
#include "stats.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#define UDP_PORT                  514
#define TCP_PORT                  514
#define NETWORK_PORT              514
#define SYSLOG_TRANSPORT_UDP_PORT 514
#define SYSLOG_TRANSPORT_TCP_PORT 601
#define SYSLOG_TRANSPORT_TLS_PORT 6514

void
transport_mapper_inet_init_instance(TransportMapperInet *self, const gchar *transport)
{
  transport_mapper_init_instance(&self->super, transport);
  self->super.address_family = AF_INET;
}

TransportMapperInet *
transport_mapper_inet_new_instance(const gchar *transport)
{
  TransportMapperInet *self = g_new0(TransportMapperInet, 1);

  transport_mapper_inet_init_instance(self, transport);
  return self;
}

TransportMapper *
transport_mapper_tcp_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.sock_type = SOCK_STREAM;
  self->super.sock_proto = IPPROTO_TCP;
  self->super.logproto = "text";
  self->super.stats_source = SCS_TCP;
  self->server_port = TCP_PORT;
  return &self->super;
}

TransportMapper *
transport_mapper_tcp6_new(void)
{
  TransportMapper *self = transport_mapper_tcp_new();

  self->address_family = AF_INET6;
  self->stats_source = SCS_TCP6;
  return self;
}

TransportMapper *
transport_mapper_udp_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("udp");

  self->super.sock_type = SOCK_DGRAM;
  self->super.sock_proto = IPPROTO_UDP;
  self->super.logproto = "dgram";
  self->super.stats_source = SCS_UDP;
  self->server_port = UDP_PORT;
  return &self->super;
}

TransportMapper *
transport_mapper_udp6_new(void)
{
  TransportMapper *self = transport_mapper_udp_new();

  self->address_family = AF_INET6;
  self->stats_source = SCS_UDP6;
  return self;
}

static gboolean
transport_mapper_network_apply_transport(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;
  const gchar *transport;

  /* determine default port, apply transport setting to afsocket flags */

  if (!transport_mapper_apply_transport_method(s, cfg))
    return FALSE;

  transport = self->super.transport;
  self->server_port = NETWORK_PORT;
  if (strcasecmp(transport, "udp") == 0)
    {
      self->super.sock_type = SOCK_DGRAM;
      self->super.sock_proto = IPPROTO_UDP;
      self->super.logproto = "dgram";
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      self->super.logproto = "text";
      self->require_tls = TRUE;
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
    }
  else
    {
      self->super.logproto = self->super.transport;
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      /* FIXME: look up port/protocol from the logproto */
      self->server_port = TCP_PORT;
    }

  g_assert(self->server_port != 0);

  return TRUE;
}

TransportMapper *
transport_mapper_network_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.apply_transport = transport_mapper_network_apply_transport;
  self->super.stats_source = SCS_NETWORK;
  return &self->super;
}

static gboolean
transport_mapper_syslog_apply_transport(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;
  const gchar *transport = self->super.transport;

  /* determine default port, apply transport setting to afsocket flags */

  if (!transport_mapper_apply_transport_method(s, cfg))
    return FALSE;

  if (strcasecmp(transport, "udp") == 0)
    {
      if (cfg_is_config_version_older(cfg, 0x0303))
        {
          self->server_port_change_warning = "WARNING: Default port for syslog(transport(udp)) has changed from 601 to 514 in " VERSION_3_3 ", please update your configuration";
          self->server_port = 601;
        }
      else
        self->server_port = SYSLOG_TRANSPORT_UDP_PORT;

      self->super.sock_type = SOCK_DGRAM;
      self->super.sock_proto = IPPROTO_UDP;
      self->super.logproto = "dgram";
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->server_port = SYSLOG_TRANSPORT_TCP_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      if (cfg_is_config_version_older(cfg, 0x0303))
        {
          self->server_port_change_warning = "WARNING: Default port for syslog(transport(tls))  has changed from 601 to 6514 in " VERSION_3_3 ", please update your configuration";
          self->server_port = 601;
        }
      else
        self->server_port = SYSLOG_TRANSPORT_TLS_PORT;
      self->super.logproto = "framed";
      self->require_tls = TRUE;
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
    }
  else
    {
      self->super.logproto = self->super.transport;
      self->super.sock_type = SOCK_STREAM;
      /* FIXME: look up port/protocol from the logproto */
      self->server_port = 514;
      self->super.sock_proto = IPPROTO_TCP;
    }
  g_assert(self->server_port != 0);

  return TRUE;
}

TransportMapper *
transport_mapper_syslog_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.apply_transport = transport_mapper_syslog_apply_transport;
  self->super.stats_source = SCS_SYSLOG;
  return &self->super;
}
