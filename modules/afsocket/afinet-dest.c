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

#include "afinet-dest.h"
#include "transport-mapper-inet.h"
#include "socket-options-inet.h"
#include "messages.h"
#include "gprocess.h"
#include "compat/openssl_support.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#ifdef _GNU_SOURCE
#  define _GNU_SOURCE_DEFINED 1
#  undef _GNU_SOURCE
#endif

#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
#include <libnet.h>
#endif

#if _GNU_SOURCE_DEFINED
#  undef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif

void
afinet_dd_set_localip(LogDriver *s, gchar *ip)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->bind_ip)
    g_free(self->bind_ip);
  self->bind_ip = g_strdup(ip);
}

void
afinet_dd_set_localport(LogDriver *s, gchar *service)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->bind_port)
    g_free(self->bind_port);
  self->bind_port = g_strdup(service);
}

void
afinet_dd_set_destport(LogDriver *s, gchar *service)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (self->dest_port)
    g_free(self->dest_port);
  self->dest_port = g_strdup(service);
}

void
afinet_dd_set_spoof_source(LogDriver *s, gboolean enable)
{
#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  self->spoof_source = enable;
#else
  msg_error("Error enabling spoof-source, you need to compile syslog-ng with --enable-spoof-source");
#endif
}

static gint
afinet_dd_verify_callback(gint ok, X509_STORE_CTX *ctx, gpointer user_data)
{
  AFInetDestDriver *self G_GNUC_UNUSED = (AFInetDestDriver *) user_data;
  TransportMapperInet *transport_mapper_inet = (TransportMapperInet *) self->super.transport_mapper;

  X509 *current_cert = X509_STORE_CTX_get_current_cert(ctx);
  X509 *cert = X509_STORE_CTX_get0_cert(ctx);

  if (ok && current_cert == cert && self->hostname
      && (tls_context_get_verify_mode(transport_mapper_inet->tls_context) & TVM_TRUSTED))
    {
      ok = tls_verify_certificate_name(cert, self->hostname);
    }

  return ok;
}

void
afinet_dd_set_tls_context(LogDriver *s, TLSContext *tls_context)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;
  transport_mapper_inet_set_tls_context((TransportMapperInet *) self->super.transport_mapper, tls_context,
                                        afinet_dd_verify_callback, self);
}

static LogWriter *
afinet_dd_construct_writer(AFSocketDestDriver *s)
{
  AFInetDestDriver *self G_GNUC_UNUSED = (AFInetDestDriver *) s;
  LogWriter *writer;
  TransportMapperInet *transport_mapper_inet G_GNUC_UNUSED = ((TransportMapperInet *) (self->super.transport_mapper));

  writer = afsocket_dd_construct_writer_method(s);
  /* SSL is duplex, so we can certainly expect input from the server, which
   * would cause the LogWriter to close this connection.  In a better world
   * LW_DETECT_EOF would be implemented by the LogProto class and would
   * inherently work w/o mockery in LogWriter.  Defer that change for now
   * (and possibly for all eternity :)
   */

  if (((self->super.transport_mapper->sock_type == SOCK_STREAM) && transport_mapper_inet->tls_context))
    log_writer_set_flags(writer, log_writer_get_flags(writer) & ~LW_DETECT_EOF);

  return writer;
}

static const gint
_determine_port(const AFInetDestDriver *self)
{
  gint port = 0;

  if (!self->dest_port)
    port = transport_mapper_inet_get_server_port(self->super.transport_mapper);
  else
    port = afinet_lookup_service(self->super.transport_mapper, self->dest_port);

  return port;
}

static gboolean
afinet_dd_setup_addresses(AFSocketDestDriver *s)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  if (!afsocket_dd_setup_addresses_method(s))
    return FALSE;

  g_sockaddr_unref(self->super.bind_addr);
  g_sockaddr_unref(self->super.dest_addr);

  if (!resolve_hostname_to_sockaddr(&self->super.bind_addr, self->super.transport_mapper->address_family, self->bind_ip))
    return FALSE;

  if (self->bind_port)
    g_sockaddr_set_port(self->super.bind_addr, afinet_lookup_service(self->super.transport_mapper, self->bind_port));

  if (!resolve_hostname_to_sockaddr(&self->super.dest_addr, self->super.transport_mapper->address_family, self->hostname))
    return FALSE;

  if (!self->dest_port)
    {
      const gchar *port_change_warning = transport_mapper_inet_get_port_change_warning(self->super.transport_mapper);

      if (port_change_warning)
        {
          msg_warning(port_change_warning,
                      evt_tag_str("id", self->super.super.super.id));
        }
    }

  g_sockaddr_set_port(self->super.dest_addr, _determine_port(self));

  return TRUE;
}

static const gchar *
afinet_dd_get_dest_name(const AFSocketDestDriver *s)
{
  const AFInetDestDriver *self = (const AFInetDestDriver *)s;
  static gchar buf[256];

  if (strchr(self->hostname, ':') != NULL)
    g_snprintf(buf, sizeof(buf), "[%s]:%d", self->hostname, _determine_port(self));
  else
    g_snprintf(buf, sizeof(buf), "%s:%d", self->hostname, _determine_port(self));
  return buf;
}

static gboolean
afinet_dd_init(LogPipe *s)
{
  AFInetDestDriver *self G_GNUC_UNUSED = (AFInetDestDriver *) s;

#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  if (self->spoof_source)
    self->super.connections_kept_alive_across_reloads = TRUE;
#endif

  if (!afsocket_dd_init(s))
    return FALSE;

#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  if (self->super.transport_mapper->sock_type == SOCK_DGRAM)
    {
      if (self->spoof_source && !self->lnet_ctx)
        {
          gchar error[LIBNET_ERRBUF_SIZE];
          cap_t saved_caps;

          saved_caps = g_process_cap_save();
          g_process_cap_modify(CAP_NET_RAW, TRUE);
          self->lnet_ctx = libnet_init(self->super.bind_addr->sa.sa_family == AF_INET ? LIBNET_RAW4 : LIBNET_RAW6, NULL, error);
          g_process_cap_restore(saved_caps);
          if (!self->lnet_ctx)
            {
              msg_error("Error initializing raw socket, spoof-source support disabled",
                        evt_tag_str("error", NULL));
            }
        }
    }
#endif

  return TRUE;
}

#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
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

#if SYSLOG_NG_ENABLE_IPV6
static gboolean
afinet_dd_construct_ipv6_packet(AFInetDestDriver *self, LogMessage *msg, GString *msg_line)
{
  libnet_ptag_t ip, udp;
  struct sockaddr_in *src4;
  struct sockaddr_in6 src, *dst;
  struct libnet_in6_addr ln_src, ln_dst;

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

  memcpy(&ln_src, &src.sin6_addr, sizeof(ln_src));
  memcpy(&ln_dst, &dst->sin6_addr, sizeof(ln_dst));
  ip = libnet_build_ipv6(0, 0,
                         LIBNET_UDP_H + msg_line->len,
                         IPPROTO_UDP,            /* IPv6 next header */
                         64,                     /* hop limit */
                         ln_src, ln_dst,
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
afinet_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  /* NOTE: this code should probably become a LogTransport instance so that
   * spoofed packets are also going through the LogWriter queue */

  if (self->spoof_source && self->lnet_ctx && msg->saddr && (msg->saddr->sa.sa_family == AF_INET
                                                             || msg->saddr->sa.sa_family == AF_INET6) && log_writer_opened(self->super.writer))
    {
      gboolean success = FALSE;

      g_assert(self->super.transport_mapper->sock_type == SOCK_DGRAM);

      g_static_mutex_lock(&self->lnet_lock);

      if (!self->lnet_buffer)
        self->lnet_buffer = g_string_sized_new(self->spoof_source_maxmsglen);

      log_writer_format_log(self->super.writer, msg, self->lnet_buffer);

      if (self->lnet_buffer->len > self->spoof_source_maxmsglen)
        g_string_truncate(self->lnet_buffer, self->spoof_source_maxmsglen);

      switch (self->super.dest_addr->sa.sa_family)
        {
        case AF_INET:
          success = afinet_dd_construct_ipv4_packet(self, msg, self->lnet_buffer);
          break;
#if SYSLOG_NG_ENABLE_IPV6
        case AF_INET6:
          success = afinet_dd_construct_ipv6_packet(self, msg, self->lnet_buffer);
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
              log_msg_ack(msg, path_options, AT_PROCESSED);
              log_msg_unref(msg);

              g_static_mutex_unlock(&self->lnet_lock);
              return;
            }
          else
            {
              msg_error("Error sending raw frame",
                        evt_tag_str("error", libnet_geterror(self->lnet_ctx)));
            }
        }
      g_static_mutex_unlock(&self->lnet_lock);
    }
#endif
  log_dest_driver_queue_method(s, msg, path_options, user_data);
}

void
afinet_dd_free(LogPipe *s)
{
  AFInetDestDriver *self = (AFInetDestDriver *) s;

  g_free(self->hostname);
  g_free(self->bind_ip);
  g_free(self->bind_port);
  g_free(self->dest_port);
#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  if (self->lnet_buffer)
    g_string_free(self->lnet_buffer, TRUE);
  g_static_mutex_free(&self->lnet_lock);
#endif
  afsocket_dd_free(s);
}


static AFInetDestDriver *
afinet_dd_new_instance(TransportMapper *transport_mapper, gchar *hostname, GlobalConfig *cfg)
{
  AFInetDestDriver *self = g_new0(AFInetDestDriver, 1);

  afsocket_dd_init_instance(&self->super, socket_options_inet_new(), transport_mapper, cfg);
  self->super.super.super.super.init = afinet_dd_init;
  self->super.super.super.super.queue = afinet_dd_queue;
  self->super.super.super.super.free_fn = afinet_dd_free;
  self->super.construct_writer = afinet_dd_construct_writer;
  self->super.setup_addresses = afinet_dd_setup_addresses;
  self->super.get_dest_name = afinet_dd_get_dest_name;

  self->hostname = g_strdup(hostname);

#if SYSLOG_NG_ENABLE_SPOOF_SOURCE
  g_static_mutex_init(&self->lnet_lock);
  self->spoof_source_maxmsglen = 1024;
#endif
  return self;
}

AFInetDestDriver *
afinet_dd_new_tcp(gchar *host, GlobalConfig *cfg)
{
  return afinet_dd_new_instance(transport_mapper_tcp_new(), host, cfg);
}

AFInetDestDriver *
afinet_dd_new_tcp6(gchar *host, GlobalConfig *cfg)
{
  return afinet_dd_new_instance(transport_mapper_tcp6_new(), host, cfg);
}

AFInetDestDriver *
afinet_dd_new_udp(gchar *host, GlobalConfig *cfg)
{
  return afinet_dd_new_instance(transport_mapper_udp_new(), host, cfg);
}

AFInetDestDriver *
afinet_dd_new_udp6(gchar *host, GlobalConfig *cfg)
{
  return afinet_dd_new_instance(transport_mapper_udp6_new(), host, cfg);
}

static LogWriter *
afinet_dd_syslog_construct_writer(AFSocketDestDriver *s)
{
  AFInetDestDriver *self G_GNUC_UNUSED = (AFInetDestDriver *) s;
  LogWriter *writer;

  writer = afsocket_dd_construct_writer_method(s);
  log_writer_set_flags(writer, log_writer_get_flags(writer) | LW_SYSLOG_PROTOCOL);
  return writer;
}


AFInetDestDriver *
afinet_dd_new_syslog(gchar *host, GlobalConfig *cfg)
{
  AFInetDestDriver *self = afinet_dd_new_instance(transport_mapper_syslog_new(), host, cfg);

  self->super.construct_writer = afinet_dd_syslog_construct_writer;
  return self;
}

AFInetDestDriver *
afinet_dd_new_network(gchar *host, GlobalConfig *cfg)
{
  return afinet_dd_new_instance(transport_mapper_network_new(), host, cfg);
}
