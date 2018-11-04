/*
 * Copyright (c) 2002-2013 Balabit
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
#include "socket-options-inet.h"
#include "gsockaddr.h"
#include "messages.h"
#include "gprocess.h"

#include <string.h>
#include <netinet/tcp.h>

#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifdef SO_BINDTODEVICE
static int
socket_options_inet_set_interface(SocketOptionsInet *self, gint fd)
{
  cap_t saved_caps;
  int result, rc;

  saved_caps = g_process_cap_save();
  g_process_enable_cap("cap_net_raw");
  rc = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, self->interface_name, strlen(self->interface_name));
  result = (rc < 0) ? errno : 0;
  g_process_cap_restore(saved_caps);
  return result;
}

static gboolean
socket_options_inet_try_to_set_interface(SocketOptionsInet *self, gint fd)
{
  int result = socket_options_inet_set_interface(self, fd);
  if (result != 0)
    {
      msg_error("Can't bind to interface", evt_tag_str("interface", self->interface_name), evt_tag_errno("error", result));
      return FALSE;
    }
  return TRUE;
}
#else
static gboolean
socket_options_inet_try_to_set_interface(SocketOptionsInet *self, gint fd)
{
  msg_error("interface() is set but no SO_BINDTODEVICE setsockopt on this platform");
  return FALSE;
}
#endif

static gboolean
socket_options_inet_setup_socket(SocketOptions *s, gint fd, GSockAddr *addr, AFSocketDirection dir)
{
  SocketOptionsInet *self = (SocketOptionsInet *) s;
  gint off = 0;
  gint on = 1;

  if (!socket_options_setup_socket_method(s, fd, addr, dir))
    return FALSE;

  if (self->tcp_keepalive_time > 0)
    {
#ifdef TCP_KEEPIDLE
      setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &self->tcp_keepalive_time, sizeof(self->tcp_keepalive_time));
#else
      msg_error("tcp-keepalive-time() is set but no TCP_KEEPIDLE setsockopt on this platform");
      return FALSE;
#endif
    }
  if (self->tcp_keepalive_probes > 0)
    {
#ifdef TCP_KEEPCNT
      setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &self->tcp_keepalive_probes, sizeof(self->tcp_keepalive_probes));
#else
      msg_error("tcp-keepalive-probes() is set but no TCP_KEEPCNT setsockopt on this platform");
      return FALSE;
#endif
    }
  if (self->tcp_keepalive_intvl > 0)
    {
#ifdef TCP_KEEPINTVL
      setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &self->tcp_keepalive_intvl, sizeof(self->tcp_keepalive_intvl));
#else
      msg_error("tcp-keepalive-intvl() is set but no TCP_KEEPINTVL setsockopt on this platform");
      return FALSE;
#endif
    }

  if (self->interface_name)
    {
      if (!socket_options_inet_try_to_set_interface(self, fd))
        return FALSE;
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
              if (self->ip_ttl)
                setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &self->ip_ttl, sizeof(self->ip_ttl));
            }

        }
      else
        {
          if (self->ip_ttl && (dir & AFSOCKET_DIR_SEND))
            setsockopt(fd, SOL_IP, IP_TTL, &self->ip_ttl, sizeof(self->ip_ttl));
        }
      if (self->ip_tos && (dir & AFSOCKET_DIR_SEND))
        setsockopt(fd, SOL_IP, IP_TOS, &self->ip_tos, sizeof(self->ip_tos));

      if (self->ip_freebind && (dir & AFSOCKET_DIR_RECV))
        {
#ifdef IP_FREEBIND
          setsockopt(fd, SOL_IP, IP_FREEBIND, &on, sizeof(on));
#else
          msg_error("ip-freebind() is set but no IP_FREEBIND setsockopt on this platform");
          return FALSE;
#endif
        }
      break;
    }
#if SYSLOG_NG_ENABLE_IPV6
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
              if (self->ip_ttl)
                setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_HOPS, &self->ip_ttl, sizeof(self->ip_ttl));
            }
        }
      else
        {
          if (self->ip_ttl && (dir & AFSOCKET_DIR_SEND))
            setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &self->ip_ttl, sizeof(self->ip_ttl));
        }

      if (self->ip_freebind && (dir & AFSOCKET_DIR_RECV))
        {
#ifdef IP_FREEBIND
          /* NOTE: there's no separate IP_FREEBIND option for IPV6, it re-uses the IPv4 one */
          setsockopt(fd, SOL_IP, IP_FREEBIND, &on, sizeof(on));
#else
          msg_error("ip-freebind() is set but no IP_FREEBIND setsockopt on this platform");
          return FALSE;
#endif
        }
      break;
    }
#endif
    default:
      g_assert_not_reached();
      break;
    }
  return TRUE;
}

void
socket_options_inet_set_interface_name(SocketOptionsInet *self, const gchar *interface_name)
{
  g_free(self->interface_name);
  self->interface_name = g_strdup(interface_name);
}

void
socket_options_inet_free(gpointer s)
{
  SocketOptionsInet *self = (SocketOptionsInet *) s;
  g_free(self->interface_name);
  g_free(self);
}

SocketOptionsInet *
socket_options_inet_new_instance(void)
{
  SocketOptionsInet *self = g_new0(SocketOptionsInet, 1);

  socket_options_init_instance(&self->super);
  self->super.setup_socket = socket_options_inet_setup_socket;
  self->super.so_keepalive = TRUE;
#if defined(TCP_KEEPTIME) && defined(TCP_KEEPIDLE) && defined(TCP_KEEPCNT)
  self->tcp_keepalive_time = 60;
  self->tcp_keepalive_intvl = 10;
  self->tcp_keepalive_probes = 6;
#endif
  self->super.free = socket_options_inet_free;
  return self;
}

SocketOptions *
socket_options_inet_new(void)
{
  return &socket_options_inet_new_instance()->super;
}
