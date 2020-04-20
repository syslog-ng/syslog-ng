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
#include "afinet.h"
#include "messages.h"
#include "gprocess.h"
#include "transport-mapper-inet.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

static gint
afinet_lookup_service_and_proto(const gchar *service, const gchar *proto)
{
  gchar *end;
  gint port;

  /* check if service is numeric */
  port = strtol(service, &end, 10);
  if ((*end != 0))
    {
      struct servent *se;

      /* service is not numeric, check if it's a service in /etc/services */
      se = getservbyname(service, proto);
      if (se)
        {
          port = ntohs(se->s_port);
        }
      else
        {
          msg_error("Error finding port number, falling back to default",
                    evt_tag_printf("service", "%s/%s", proto, service));
          return 0;
        }
    }
  return port;
}

static const gchar *
afinet_lookup_proto(gint protocol_number, gint sock_type)
{
  struct protoent *ipproto_ent = getprotobynumber(protocol_number);

  return ipproto_ent ? ipproto_ent->p_name
         : ((sock_type == SOCK_STREAM) ? "tcp" : "udp");
}

guint16
afinet_lookup_service(const TransportMapper *transport_mapper, const gchar *service)
{
  const gchar *protocol_name = afinet_lookup_proto(transport_mapper->sock_proto, transport_mapper->sock_type);

  return afinet_lookup_service_and_proto(service, protocol_name);
}

gint
afinet_determine_port(const TransportMapper *transport_mapper, const gchar *service_port)
{
  gint port = 0;

  if (!service_port)
    port = transport_mapper_inet_get_server_port(transport_mapper);
  else
    port = afinet_lookup_service(transport_mapper, service_port);

  return port;
}
