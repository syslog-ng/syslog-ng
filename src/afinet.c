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
              g_sockaddr_inet6_set_address(addr, ((struct sockaddr_in6 *) res->ai_addr)->sin6_addr);
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

LogDriver *
afinet_sd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetSourceDriver *self = g_new0(AFInetSourceDriver, 1);
  
  afsocket_sd_init_instance(&self->super, flags);
  self->super.flags |= AFSOCKET_KEEP_ALIVE;
  if (af == AF_INET)
    {
      self->super.bind_addr = g_sockaddr_inet_new("0.0.0.0", port);
      if (!host)
        host = "0.0.0.0";
    }
  else
    {
      self->super.bind_addr = g_sockaddr_inet6_new("::", port);
      if (!host)
        host = "::";
    }
  afinet_resolve_name(self->super.bind_addr, host);
    
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

LogDriver *
afinet_dd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetDestDriver *self = g_new0(AFInetDestDriver, 1);
  
  afsocket_dd_init_instance(&self->super, flags);
  if (af == AF_INET)
    {
      self->super.bind_addr = g_sockaddr_inet_new("0.0.0.0", 0);
      self->super.dest_addr = g_sockaddr_inet_new("0.0.0.0", port);
    }
  else
    {
      self->super.bind_addr = g_sockaddr_inet6_new("::", 0);
      self->super.dest_addr = g_sockaddr_inet6_new("::", port);
    }
  afinet_resolve_name(self->super.dest_addr, host);
  return &self->super.super;
}
