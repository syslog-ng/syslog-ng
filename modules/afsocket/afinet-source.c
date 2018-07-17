/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afinet-source.h"
#include "messages.h"
#include "transport-mapper-inet.h"
#include "socket-options-inet.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

void
afinet_sd_set_localport(LogDriver *s, gchar *service)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  if (self->bind_port)
    g_free(self->bind_port);
  self->bind_port = g_strdup(service);
}

void
afinet_sd_set_localip(LogDriver *s, gchar *ip)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  if (self->bind_ip)
    g_free(self->bind_ip);
  self->bind_ip = g_strdup(ip);
}

void
afinet_sd_set_tls_context(LogDriver *s, TLSContext *tls_context)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  transport_mapper_inet_set_tls_context((TransportMapperInet *) self->super.transport_mapper, tls_context, NULL);
}

static gboolean
afinet_sd_setup_addresses(AFSocketSourceDriver *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  if (!afsocket_sd_setup_addresses_method(s))
    return FALSE;

  if (self->super.proto_factory->default_inet_port)
    transport_mapper_inet_set_server_port(self->super.transport_mapper, self->super.proto_factory->default_inet_port);

  g_sockaddr_unref(self->super.bind_addr);

  if (!resolve_hostname_to_sockaddr(&self->super.bind_addr, self->super.transport_mapper->address_family, self->bind_ip))
    return FALSE;

  if (!self->bind_port)
    {
      const gchar *port_change_warning = transport_mapper_inet_get_port_change_warning(self->super.transport_mapper);

      if (port_change_warning)
        {
          msg_warning(port_change_warning,
                      evt_tag_str("id", self->super.super.super.id));
        }
      g_sockaddr_set_port(self->super.bind_addr, transport_mapper_inet_get_server_port(self->super.transport_mapper));
    }
  else
    g_sockaddr_set_port(self->super.bind_addr, afinet_lookup_service(self->super.transport_mapper, self->bind_port));

  return TRUE;
}

gboolean
afinet_sd_init(LogPipe *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  if (!afsocket_sd_init_method(&self->super.super.super.super))
    return FALSE;

  return TRUE;
}

void
afinet_sd_free(LogPipe *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  g_free(self->bind_ip);
  g_free(self->bind_port);
  afsocket_sd_free_method(s);
}

static AFInetSourceDriver *
afinet_sd_new_instance(TransportMapper *transport_mapper, GlobalConfig *cfg)
{
  AFInetSourceDriver *self = g_new0(AFInetSourceDriver, 1);

  afsocket_sd_init_instance(&self->super,
                            socket_options_inet_new(),
                            transport_mapper,
                            cfg);
  self->super.super.super.super.init = afinet_sd_init;
  self->super.super.super.super.free_fn = afinet_sd_free;
  self->super.setup_addresses = afinet_sd_setup_addresses;
  return self;
}

AFInetSourceDriver *
afinet_sd_new_tcp(GlobalConfig *cfg)
{
  return afinet_sd_new_instance(transport_mapper_tcp_new(), cfg);
}

AFInetSourceDriver *
afinet_sd_new_tcp6(GlobalConfig *cfg)
{
  return afinet_sd_new_instance(transport_mapper_tcp6_new(), cfg);
}


AFInetSourceDriver *
afinet_sd_new_udp(GlobalConfig *cfg)
{
  return afinet_sd_new_instance(transport_mapper_udp_new(), cfg);
}

AFInetSourceDriver *
afinet_sd_new_udp6(GlobalConfig *cfg)
{
  return afinet_sd_new_instance(transport_mapper_udp6_new(), cfg);
}


AFInetSourceDriver *
afinet_sd_new_syslog(GlobalConfig *cfg)
{
  AFInetSourceDriver *self = afinet_sd_new_instance(transport_mapper_syslog_new(), cfg);

  self->super.reader_options.parse_options.flags |= LP_SYSLOG_PROTOCOL;
  return self;
}

AFInetSourceDriver *
afinet_sd_new_network(GlobalConfig *cfg)
{
  AFInetSourceDriver *self = afinet_sd_new_instance(transport_mapper_network_new(), cfg);
  return self;
}
