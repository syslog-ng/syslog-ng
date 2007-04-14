/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "afinet.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>

#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif


static void
afinet_set_port(GSockAddr *addr, gint port, gchar *service, gchar *proto)
{
  if (addr)
    {
      if (proto)
        {
          struct servent *se;
          
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

static void
afinet_resolve_name(GSockAddr *addr, gchar *name)
{
  if (addr)
    { 
#if HAVE_GETADDRINFO
      struct addrinfo hints;
      struct addrinfo *res;

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = addr->sa.sa_family;
      hints.ai_socktype = 0;
      hints.ai_protocol = 0;
      
      if (getaddrinfo(name, NULL, &hints, &res) == 0)
        {
          /* we only use the first entry in the returned list */
          switch (addr->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address(addr, ((struct sockaddr_in *) res->ai_addr)->sin_addr);
              break;
#if ENABLE_IPV6
            case AF_INET6:
              g_sockaddr_inet6_set_address(addr, &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr);
              break;
#endif
            default: 
              g_assert_not_reached();
              break;
            }
          freeaddrinfo(res);
        }
      else
        {
          msg_error("Error resolving hostname, using wildcard address", evt_tag_str("host", name), NULL);
        }
#else
      struct hostent *he;
      
      he = gethostbyname(name);
      if (he)
        {
          switch (addr->sa.sa_family)
            {
            case AF_INET:
              g_sockaddr_inet_set_address(addr, *(struct in_addr *) he->h_addr);
              break;
            default: 
              g_assert_not_reached();
              break;
            }
          
        }
      else
        {
          msg_error("Error resolving hostname, using wildcard address", evt_tag_str("host", name), NULL);
        }
#endif
    }
}

static gboolean
afinet_setup_socket(gint fd, GSockAddr *addr, InetSocketOptions *sock_options, AFSocketDirection dir)
{
  gint off = 0;
  
  if (!afsocket_setup_socket(fd, &sock_options->super, dir))
    return FALSE;

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
                if (sock_options->ttl)
                  setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &sock_options->ttl, sizeof(sock_options->ttl));
              }
            
          }
        else
          {
            if (sock_options->ttl && (dir & AFSOCKET_DIR_SEND))
              setsockopt(fd, SOL_IP, IP_TTL, &sock_options->ttl, sizeof(sock_options->ttl));
          }        
        if (sock_options->tos && (dir & AFSOCKET_DIR_SEND))
          setsockopt(fd, SOL_IP, IP_TOS, &sock_options->tos, sizeof(sock_options->tos));
          
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
                if (sock_options->ttl)
                  setsockopt(fd, SOL_IPV6, IPV6_MULTICAST_HOPS, &sock_options->ttl, sizeof(sock_options->ttl));
              }
          }
        else
          {
            if (sock_options->ttl && (dir & AFSOCKET_DIR_SEND))
              setsockopt(fd, SOL_IPV6, IPV6_UNICAST_HOPS, &sock_options->ttl, sizeof(sock_options->ttl));
          }
        break;
      }
#endif
    }
  return TRUE;
}

void 
afinet_sd_set_localport(LogDriver *s, gint port, gchar *service, gchar *proto)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  afinet_set_port(self->bind_addr, port, service, proto);
}

void 
afinet_sd_set_localip(LogDriver *s, gchar *ip)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  afinet_resolve_name(self->bind_addr, ip);
}

static gboolean
afinet_sd_setup_socket(AFSocketSourceDriver *s, gint fd)
{
  return afinet_setup_socket(fd, s->bind_addr, (InetSocketOptions *) s->sock_options_ptr, AFSOCKET_DIR_RECV);
}

LogDriver *
afinet_sd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetSourceDriver *self = g_new0(AFInetSourceDriver, 1);
  
  afsocket_sd_init_instance(&self->super, &self->sock_options.super, flags);
  self->super.flags |= AFSOCKET_KEEP_ALIVE;
  if (af == AF_INET)
    {
      self->super.bind_addr = g_sockaddr_inet_new("0.0.0.0", port);
      if (!host)
        host = "0.0.0.0";
    }
  else
    {
#if ENABLE_IPV6
      self->super.bind_addr = g_sockaddr_inet6_new("::", port);
      if (!host)
        host = "::";
#else
      g_assert_not_reached();
#endif
    }
  afinet_resolve_name(self->super.bind_addr, host);
  self->super.setup_socket = afinet_sd_setup_socket;
    
  return &self->super.super;
}

/* afinet destination */

void 
afinet_dd_set_localport(LogDriver *s, gint port, gchar *service, gchar *proto)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  afinet_set_port(self->super.bind_addr, port, service, proto);
}

void 
afinet_dd_set_destport(LogDriver *s, gint port, gchar *service, gchar *proto)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  afinet_set_port(self->super.dest_addr, port, service, proto);
}

void 
afinet_dd_set_localip(LogDriver *s, gchar *ip)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  afinet_resolve_name(self->super.bind_addr, ip);
}

static gboolean
afinet_dd_setup_socket(AFSocketDestDriver *s, gint fd)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  return afinet_setup_socket(fd, self->super.dest_addr, (InetSocketOptions *) s->sock_options_ptr, AFSOCKET_DIR_SEND);
}

LogDriver *
afinet_dd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetDestDriver *self = g_new0(AFInetDestDriver, 1);
  
  afsocket_dd_init_instance(&self->super, &self->sock_options.super, flags);
  if (af == AF_INET)
    {
      self->super.bind_addr = g_sockaddr_inet_new("0.0.0.0", 0);
      self->super.dest_addr = g_sockaddr_inet_new("0.0.0.0", port);
    }
  else
    {
#if ENABLE_IPV6
      self->super.bind_addr = g_sockaddr_inet6_new("::", 0);
      self->super.dest_addr = g_sockaddr_inet6_new("::", port);
#else
      g_assert_not_reached();
#endif
    }
  afinet_resolve_name(self->super.dest_addr, host);
  self->super.setup_socket = afinet_dd_setup_socket;
  return &self->super.super;
}
