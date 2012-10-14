/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
#include "afinet.h"
#include "messages.h"
#include "misc.h"
#include "gprocess.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>


#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif

void
afinet_set_port(GSockAddr *addr, gchar *service, const gchar *proto)
{
  if (addr)
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
                        evt_tag_printf("service", "%s/%s", proto, service),
                        NULL);
              return;
            }
        }

      switch (addr->sa.sa_family)
        {
        case AF_INET:
          g_sockaddr_inet_set_port(addr, port);
          break;
#if ENABLE_IPV6
        case AF_INET6:
          g_sockaddr_inet6_set_port(addr, port);
          break;
#endif
        default:
          g_assert_not_reached();
          break;
        }
    }
}

gboolean
afinet_setup_socket(gint fd, GSockAddr *addr, InetSocketOptions *sock_options, AFSocketDirection dir)
{
  gint off = 0;

  if (!afsocket_setup_socket(fd, &sock_options->super, dir))
    return FALSE;

  if (sock_options->tcp_keepalive_time > 0)
    {
#ifdef TCP_KEEPIDLE
      setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &sock_options->tcp_keepalive_time, sizeof(sock_options->tcp_keepalive_time));
#else
      msg_error("tcp-keepalive-time() is set but no TCP_KEEPIDLE setsockopt on this platform", NULL);
      return FALSE;
#endif
    }
  if (sock_options->tcp_keepalive_probes > 0)
    {
#ifdef TCP_KEEPCNT
      setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &sock_options->tcp_keepalive_probes, sizeof(sock_options->tcp_keepalive_probes));
#else
      msg_error("tcp-keepalive-probes() is set but no TCP_KEEPCNT setsockopt on this platform", NULL);
      return FALSE;
#endif
    }
  if (sock_options->tcp_keepalive_intvl > 0)
    {
#ifdef TCP_KEEPINTVL
      setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &sock_options->tcp_keepalive_intvl, sizeof(sock_options->tcp_keepalive_intvl));
#else
      msg_error("tcp-keepalive-intvl() is set but no TCP_KEEPINTVL setsockopt on this platform", NULL);
      return FALSE;
#endif
    }

  switch (addr->sa.sa_family)
    {
    case AF_INET:
      {
        struct ip_mreq mreq;

        if (IN_MULTICAST(ntohl(g_sockaddr_inet_get_address(addr).s_addr)))
          {
            if (dir & AFSOCKET_DIR_RECV)
              {
                memset(&mreq, 0, sizeof(mreq));
                mreq.imr_multiaddr = g_sockaddr_inet_get_address(addr);
                mreq.imr_interface.s_addr = INADDR_ANY;
                setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
                setsockopt(fd, SOL_IP, IP_MULTICAST_LOOP, &off, sizeof(off));
              }
            if (dir & AFSOCKET_DIR_SEND)
              {
                if (sock_options->ip_ttl)
                  setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &sock_options->ip_ttl, sizeof(sock_options->ip_ttl));
              }

          }
        else
          {
            if (sock_options->ip_ttl && (dir & AFSOCKET_DIR_SEND))
              setsockopt(fd, SOL_IP, IP_TTL, &sock_options->ip_ttl, sizeof(sock_options->ip_ttl));
          }
        if (sock_options->ip_tos && (dir & AFSOCKET_DIR_SEND))
          setsockopt(fd, SOL_IP, IP_TOS, &sock_options->ip_tos, sizeof(sock_options->ip_tos));

        break;
      }
#if ENABLE_IPV6
    case AF_INET6:
      {
        struct ipv6_mreq mreq6;

        if (IN6_IS_ADDR_MULTICAST(&g_sockaddr_inet6_get_sa(addr)->sin6_addr))
          {
            if (dir & AFSOCKET_DIR_RECV)
              {
                memset(&mreq6, 0, sizeof(mreq6));
                mreq6.ipv6mr_multiaddr = *g_sockaddr_inet6_get_address(addr);
                mreq6.ipv6mr_interface = 0;
                setsockopt(fd, SOL_IPV6, IPV6_JOIN_GROUP, &mreq6, sizeof(mreq6));
                setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_LOOP, &off, sizeof(off));
              }
            if (dir & AFSOCKET_DIR_SEND)
              {
                if (sock_options->ip_ttl)
                  setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_HOPS, &sock_options->ip_ttl, sizeof(sock_options->ip_ttl));
              }
          }
        else
          {
            if (sock_options->ip_ttl && (dir & AFSOCKET_DIR_SEND))
              setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &sock_options->ip_ttl, sizeof(sock_options->ip_ttl));
          }
        break;
      }
#endif
    }
  return TRUE;
}
