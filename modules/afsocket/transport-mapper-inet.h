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
#ifndef TRANSPORT_MAPPER_INET_H_INCLUDED
#define TRANSPORT_MAPPER_INET_H_INCLUDED

#include "transport-mapper.h"

typedef struct _TransportMapperInet
{
  TransportMapper super;
  gint server_port;
  const gchar *server_port_change_warning;
  gboolean require_tls;
  gboolean allow_tls;
} TransportMapperInet;

static inline gboolean
transport_mapper_inet_is_tls_required(TransportMapperInet *self)
{
  return self->require_tls;
}

static inline gboolean
transport_mapper_inet_is_tls_allowed(TransportMapperInet *self)
{
  return self->require_tls || self->allow_tls;
}

static inline gint
transport_mapper_inet_get_server_port(TransportMapper *self)
{
  return ((TransportMapperInet *) self)->server_port;
}

static inline const gchar *
transport_mapper_inet_get_port_change_warning(TransportMapper *s)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  return self->server_port_change_warning;
}

void transport_mapper_inet_init_instance(TransportMapperInet *self, const gchar *transport);
TransportMapper *transport_mapper_tcp_new(void);
TransportMapper *transport_mapper_tcp6_new(void);
TransportMapper *transport_mapper_udp_new(void);
TransportMapper *transport_mapper_udp6_new(void);
TransportMapper *transport_mapper_network_new(void);
TransportMapper *transport_mapper_syslog_new(void);

#endif
