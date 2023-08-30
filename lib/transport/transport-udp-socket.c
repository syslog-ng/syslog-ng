/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 1998-2019 Balázs Scheidler
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
#include "transport-udp-socket.h"
#include "transport/transport-socket.h"
#include "gsocket.h"
#include "scratch-buffers.h"
#include "str-format.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <string.h>

typedef struct _LogTransportUDP LogTransportUDP;
struct _LogTransportUDP
{
  LogTransportSocket super;
  GSockAddr *bind_addr;
};

#if defined(__FreeBSD__) || defined(__OpenBSD__)

GSockAddr *
_extract_dest_ip4_addr_from_cmsg(struct cmsghdr *cmsg, GSockAddr *bind_addr)
{
  if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVDSTADDR)
    {
      struct sockaddr_in sin = *(struct sockaddr_in *) &bind_addr->sa;

      struct in_addr *sin_addr = (struct in_addr *) CMSG_DATA(cmsg);
      sin.sin_addr = *sin_addr;

      return g_sockaddr_new((struct sockaddr *) &sin, sizeof(sin));
    }
  return NULL;
}

#else

GSockAddr *
_extract_dest_ip4_addr_from_cmsg(struct cmsghdr *cmsg, GSockAddr *bind_addr)
{
#ifdef IP_PKTINFO
  if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO)
    {
      struct sockaddr_in sin;
      struct in_pktinfo inpkt;
      memcpy(&inpkt, CMSG_DATA(cmsg), sizeof(inpkt));

      /* we need to copy the port number from the bind address as it is not
       * part of IP_PKTINFO */

      sin = *(struct sockaddr_in *) &bind_addr->sa;
      sin.sin_addr = inpkt.ipi_addr;
      return g_sockaddr_new((struct sockaddr *) &sin, sizeof(sin));
    }
#endif
  return NULL;
}
#endif

#if SYSLOG_NG_ENABLE_IPV6

GSockAddr *
_extract_dest_ip6_addr_from_cmsg(struct cmsghdr *cmsg, GSockAddr *bind_addr)
{
#ifdef IPV6_PKTINFO
  if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO)
    {
      struct sockaddr_in6 sin6;
      struct in6_pktinfo in6pkt;
      memcpy(&in6pkt, CMSG_DATA(cmsg), sizeof(in6pkt));

      /* we need to copy the port number (and scope id) from the bind
       * address as it is not part of IPV6_PKTINFO */

      sin6 = *(struct sockaddr_in6 *) &bind_addr->sa;
      sin6.sin6_addr = in6pkt.ipi6_addr;
      return g_sockaddr_new((struct sockaddr *) &sin6, sizeof(sin6));
    }
#endif
  return NULL;
}

#endif

GSockAddr *
_extract_dest_addr_from_cmsg(struct cmsghdr *cmsg, GSockAddr *bind_addr)
{
  if (bind_addr->sa.sa_family == AF_INET)
    return _extract_dest_ip4_addr_from_cmsg(cmsg, bind_addr);
#if SYSLOG_NG_ENABLE_IPV6
  else if (bind_addr->sa.sa_family == AF_INET6)
    return _extract_dest_ip6_addr_from_cmsg(cmsg, bind_addr);
#endif
  else
    g_assert_not_reached();
}

static void
log_transport_udp_parse_cmsg(LogTransportSocket *s, struct cmsghdr *cmsg, LogTransportAuxData *aux)
{
  LogTransportUDP *self = (LogTransportUDP *) s;

  log_transport_socket_parse_cmsg_method(s, cmsg, aux);

  GSockAddr *dest_addr = _extract_dest_addr_from_cmsg(cmsg, self->bind_addr);
  if (dest_addr)
    {
      log_transport_aux_data_set_local_addr_ref(aux, dest_addr);
      return;
    }
}

static void
_setup_fd(LogTransportUDP *self, gint fd)
{
  gint on = 1;

  self->bind_addr = g_socket_get_local_name(fd);

  if (self->super.address_family == AF_INET)
    {
#if defined(__FreeBSD__) || defined(__OpenBSD__)
      setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &on, sizeof(on));
#elif defined(IP_PKTINFO)
      setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
#endif
    }
#if SYSLOG_NG_ENABLE_IPV6 && defined(IPV6_RECVPKTINFO)
  else if (self->bind_addr->sa.sa_family == AF_INET6)
    setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
#endif
  else
    g_assert_not_reached();

}

static void
log_transport_udp_socket_free(LogTransport *s)
{
  LogTransportUDP *self = (LogTransportUDP *)s;
  g_sockaddr_unref(self->bind_addr);
  log_transport_free_method(s);
}

LogTransport *
log_transport_udp_socket_new(gint fd)
{
  LogTransportUDP *self = g_new0(LogTransportUDP, 1);

  log_transport_dgram_socket_init_instance(&self->super, fd);
  self->super.super.free_fn = log_transport_udp_socket_free;
  self->super.parse_cmsg = log_transport_udp_parse_cmsg;

  _setup_fd(self, fd);
  return &self->super.super;
}
