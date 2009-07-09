/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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


static void
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
afinet_sd_set_localport(LogDriver *s, gchar *service, const gchar *proto)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  afinet_set_port(self->bind_addr, service, proto);
}

void 
afinet_sd_set_localip(LogDriver *s, gchar *ip)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  resolve_hostname(&self->bind_addr, ip);
}

static gboolean
afinet_sd_setup_socket(AFSocketSourceDriver *s, gint fd)
{
  return afinet_setup_socket(fd, s->bind_addr, (InetSocketOptions *) s->sock_options_ptr, AFSOCKET_DIR_RECV);
}

void
afinet_sd_set_transport(LogDriver *s, const gchar *transport)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;
  
  if (self->super.transport)
    g_free(self->super.transport);
  self->super.transport = g_strdup(transport);
  if (strcasecmp(transport, "udp") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_DGRAM;
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_STREAM;
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_STREAM | AFSOCKET_REQUIRE_TLS;
    }
  else
    {
      msg_error("Unknown syslog transport specified, please use one of udp, tcp, or tls", 
                evt_tag_str("transport", transport),
                NULL);
    }
}

LogDriver *
afinet_sd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetSourceDriver *self = g_new0(AFInetSourceDriver, 1);
  
  afsocket_sd_init_instance(&self->super, &self->sock_options.super, flags);
  if (self->super.flags & AFSOCKET_DGRAM)
    self->super.transport = g_strdup("udp");
  else if (self->super.flags & AFSOCKET_STREAM)
    self->super.transport = g_strdup("tcp");
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
  resolve_hostname(&self->super.bind_addr, host);
  self->super.setup_socket = afinet_sd_setup_socket;
    
  return &self->super.super;
}

/* afinet destination */

void 
afinet_dd_set_localport(LogDriver *s, gchar *service, const gchar *proto)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  afinet_set_port(self->super.bind_addr, service, proto);
}

void 
afinet_dd_set_destport(LogDriver *s, gchar *service, const gchar *proto)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  afinet_set_port(self->super.dest_addr, service, proto);
  
  g_free(self->super.dest_name);
  self->super.dest_name = g_strdup_printf("%s:%d", self->super.hostname,
                  g_sockaddr_inet_check(self->super.dest_addr) ? g_sockaddr_inet_get_port(self->super.dest_addr)
                                                               : g_sockaddr_inet6_get_port(self->super.dest_addr));
}

void 
afinet_dd_set_localip(LogDriver *s, gchar *ip)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  resolve_hostname(&self->super.bind_addr, ip);
}

void 
afinet_dd_set_spoof_source(LogDriver *s, gboolean enable)
{
#if ENABLE_SPOOF_SOURCE
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  self->spoof_source = (self->super.flags & AFSOCKET_DGRAM) && enable;
#else
  msg_error("Error enabling spoof-source, you need to compile syslog-ng with --enable-spoof-source", NULL);
#endif
}

void
afinet_dd_set_transport(LogDriver *s, const gchar *transport)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->super.transport)
    g_free(self->super.transport);
  self->super.transport = g_strdup(transport);
  if (strcasecmp(transport, "udp") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_DGRAM;
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_STREAM;
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      self->super.flags = (self->super.flags & ~0x0003) | AFSOCKET_STREAM | AFSOCKET_REQUIRE_TLS;
    }
  else
    {
      msg_error("Unknown syslog transport specified, please use one of udp, tcp, or tls", 
                evt_tag_str("transport", transport),
                NULL);
    }
}

static gboolean
afinet_dd_setup_socket(AFSocketDestDriver *s, gint fd)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  
  if (!resolve_hostname(&self->super.dest_addr, self->super.hostname))
    return FALSE;

  return afinet_setup_socket(fd, self->super.dest_addr, (InetSocketOptions *) s->sock_options_ptr, AFSOCKET_DIR_SEND);
}

static gboolean
afinet_dd_init(LogPipe *s)
{
  AFInetDestDriver *self G_GNUC_UNUSED = (AFInetDestDriver *) s;
  gboolean success;
  
  success = afsocket_dd_init(s);
#if ENABLE_SPOOF_SOURCE
  if (success)
    {
      if (self->spoof_source && !self->lnet_ctx) 
        {
          gchar error[LIBNET_ERRBUF_SIZE];
          cap_t saved_caps;
          
          saved_caps = g_process_cap_save();
          g_process_cap_modify(CAP_NET_RAW, TRUE);
          self->lnet_ctx = libnet_init(self->super.dest_addr->sa.sa_family == AF_INET ? LIBNET_RAW4 : LIBNET_RAW6, NULL, error);
          g_process_cap_restore(saved_caps);
          if (!self->lnet_ctx) 
            {
              msg_error("Error initializing raw socket, spoof-source support disabled", 
                        evt_tag_str("error", NULL),
                        NULL);
            }
        }
    }
#endif
  
  return success;
}

#if ENABLE_SPOOF_SOURCE
static gboolean
afinet_dd_construct_ipv4_packet(AFInetDestDriver *self, LogMessage *msg, GString *msg_line)
{
  libnet_ptag_t ip, udp;
  struct sockaddr_in *src, *dst;
  
  if (msg->saddr->sa.sa_family != AF_INET)
    return FALSE;
  
  src = (struct sockaddr_in *) &msg->saddr->sa;
  dst = (struct sockaddr_in *) &self->super.dest_addr->sa;
  
  libnet_clear_packet(self->lnet_ctx);
      
  udp = libnet_build_udp(ntohs(src->sin_port), 
                         ntohs(dst->sin_port), 
                         LIBNET_UDP_H + msg_line->len,
                         0, 
                         (guchar *) msg_line->str, 
                         msg_line->len, 
                         self->lnet_ctx,
                         0);
  if (udp == -1) 
    return FALSE;
      
  ip = libnet_build_ipv4(LIBNET_IPV4_H + msg_line->len + LIBNET_UDP_H,
                         IPTOS_LOWDELAY,         /* IP tos */
                         0,                      /* IP ID */
                         0,                      /* frag stuff */
                         64,                     /* TTL */
                         IPPROTO_UDP,            /* transport protocol */
                         0,
                         src->sin_addr.s_addr,   /* source IP */
                         dst->sin_addr.s_addr,   /* destination IP */
                         NULL,                   /* payload (none) */
                         0,                      /* payload length */
                         self->lnet_ctx, 
                         0);
  if (ip == -1)
    return FALSE;
      
  return TRUE;
}

#if ENABLE_IPV6
static gboolean
afinet_dd_construct_ipv6_packet(AFInetDestDriver *self, LogMessage *msg, GString *msg_line)
{
  libnet_ptag_t ip, udp;
  struct sockaddr_in *src4;
  struct sockaddr_in6 src, *dst;
  
  switch (msg->saddr->sa.sa_family)
    {
    case AF_INET:
      src4 = (struct sockaddr_in *) &msg->saddr->sa;
      memset(&src, 0, sizeof(src));
      src.sin6_family = AF_INET6;
      src.sin6_port = src4->sin_port;
      ((guint32 *) &src.sin6_addr)[0] = 0;
      ((guint32 *) &src.sin6_addr)[1] = 0;
      ((guint32 *) &src.sin6_addr)[2] = htonl(0xffff);
      ((guint32 *) &src.sin6_addr)[3] = src4->sin_addr.s_addr;
      break;
    case AF_INET6:
      src = *((struct sockaddr_in6 *) &msg->saddr->sa);
      break;
    default:
      g_assert_not_reached();
      break;
    }
  
  dst = (struct sockaddr_in6 *) &self->super.dest_addr->sa;
  
  libnet_clear_packet(self->lnet_ctx);
      
  udp = libnet_build_udp(ntohs(src.sin6_port), 
                         ntohs(dst->sin6_port), 
                         LIBNET_UDP_H + msg_line->len,
                         0, 
                         (guchar *) msg_line->str, 
                         msg_line->len, 
                         self->lnet_ctx,
                         0);
  if (udp == -1) 
    return FALSE;
  
  /* There seems to be a bug in libnet 1.1.2 that is triggered when
   * checksumming UDP6 packets. This is a workaround below. */
  
  libnet_toggle_checksum(self->lnet_ctx, udp, LIBNET_OFF);
        
  ip = libnet_build_ipv6(0, 0,
                         LIBNET_UDP_H + msg_line->len,
                         IPPROTO_UDP,            /* IPv6 next header */
                         64,                     /* hop limit */
                         *((struct libnet_in6_addr *) &src.sin6_addr.s6_addr),  /* source IP */
                         *((struct libnet_in6_addr *) &dst->sin6_addr.s6_addr),  /* destination IP */
                         NULL, 0,                /* payload and its length */
                         self->lnet_ctx, 
                         0);
                         
  if (ip == -1)
    return FALSE;
      
  return TRUE;
}
#endif

#endif

static void
afinet_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
#if ENABLE_SPOOF_SOURCE
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->spoof_source && self->lnet_ctx && msg->saddr && (msg->saddr->sa.sa_family == AF_INET || msg->saddr->sa.sa_family == AF_INET6))
    {
      gboolean success = FALSE;
      GString *msg_line = g_string_sized_new(256);
      
      g_assert((self->super.flags & AFSOCKET_DGRAM) != 0);
      
      log_writer_format_log((LogWriter *) self->super.writer, msg, msg_line);
      
      switch (self->super.dest_addr->sa.sa_family)
        {
        case AF_INET:
          success = afinet_dd_construct_ipv4_packet(self, msg, msg_line);
          break;
#if ENABLE_IPV6
        case AF_INET6:
          success = afinet_dd_construct_ipv6_packet(self, msg, msg_line);
          break;
#endif
        default:
          g_assert_not_reached();
        }
      if (success)
        {
          if (libnet_write(self->lnet_ctx) >= 0) 
            {
              /* we have finished processing msg */
              log_msg_ack(msg, path_options);
              log_msg_unref(msg);
              g_string_free(msg_line, TRUE);
              
              return;
            }
          else
            {
              msg_error("Error sending raw frame", 
                        evt_tag_str("error", libnet_geterror(self->lnet_ctx)),
                        NULL);
            }
        }
      g_string_free(msg_line, TRUE);
    }
#endif
  log_pipe_forward_msg(s, msg, path_options);
}

void
afinet_dd_free(LogPipe *s)
{
  afsocket_dd_free(s);
}


LogDriver *
afinet_dd_new(gint af, gchar *host, gint port, guint flags)
{
  AFInetDestDriver *self = g_new0(AFInetDestDriver, 1);
  
  afsocket_dd_init_instance(&self->super, &self->sock_options.super, flags, g_strdup(host), g_strdup_printf("%s:%d", host, port));
  if (self->super.flags & AFSOCKET_DGRAM)
    self->super.transport = g_strdup("udp");
  else if (self->super.flags & AFSOCKET_STREAM)
    self->super.transport = g_strdup("tcp");
  self->super.super.super.init = afinet_dd_init;
  self->super.super.super.queue = afinet_dd_queue;
  self->super.super.super.free_fn = afinet_dd_free;
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
  self->super.setup_socket = afinet_dd_setup_socket;
  return &self->super.super;
}
