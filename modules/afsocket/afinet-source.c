/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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
#include "misc.h"
#include "gprocess.h"

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

static gboolean
afinet_sd_setup_socket(AFSocketSourceDriver *s, gint fd)
{
  return afinet_setup_socket(fd, s->bind_addr, (InetSocketOptions *) s->sock_options_ptr, AFSOCKET_DIR_RECV);
}

static gboolean
afinet_sd_apply_transport(AFSocketSourceDriver *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(&s->super.super.super);
  gchar *default_bind_ip = NULL;
  gchar *default_bind_port = NULL;
  struct protoent *ipproto_ent;

  g_sockaddr_unref(self->super.bind_addr);

  if (self->super.address_family == AF_INET)
    {
      self->super.bind_addr = g_sockaddr_inet_new("0.0.0.0", 0);
      default_bind_ip = "0.0.0.0";
    }
#if ENABLE_IPV6
  else if (self->super.address_family == AF_INET6)
    {
      self->super.bind_addr = g_sockaddr_inet6_new("::", 0);
      default_bind_ip = "::";
    }
#endif
  else
    {
      g_assert_not_reached();
    }


  /* determine default port, apply transport setting to afsocket flags */

  if (self->super.transport == NULL)
    {
      if (self->super.sock_type == SOCK_STREAM)
        afsocket_sd_set_transport(&self->super.super.super, "tcp");
      else
        afsocket_sd_set_transport(&self->super.super.super, "udp");
    }
;
  if (strcasecmp(self->super.transport, "udp") == 0)
    {
      static gboolean msg_udp_source_port_warning = FALSE;

      if (!self->bind_port)
        {
          /* NOTE: this version number change has happened in a different
           * major version in OSE vs. PE, thus the update behaviour must
           * be triggered differently.  In OSE it needs to be triggered
           * when the config version has changed to 3.3, in PE when 3.2.
           *
           * This is unfortunate, the only luck we have to be less
           * confusing is that syslog() driver was seldom used.
           *
           */
          if ((self->super.flags & AFSOCKET_SYSLOG_PROTOCOL) && cfg_is_config_version_older(cfg, 0x0303))
            {
              if (!msg_udp_source_port_warning)
                {
                  msg_warning("WARNING: Default port for syslog(transport(udp)) has changed from 601 to 514 in syslog-ng 3.3, please update your configuration",
                              evt_tag_str("id", self->super.super.super.id),
                              NULL);
                  msg_udp_source_port_warning = TRUE;
                }
              default_bind_port = "601";
            }
          else
            default_bind_port = "514";
        }
      self->super.sock_type = SOCK_DGRAM;
      self->super.logproto_name = "dgram";
    }
  else if (strcasecmp(self->super.transport, "tcp") == 0)
    {
      if (self->super.flags & AFSOCKET_SYSLOG_PROTOCOL)
        {
          default_bind_port = "601";
          self->super.logproto_name = "framed";
        }
      else
        {
          default_bind_port = "514";
          self->super.logproto_name = "text";
        }
      self->super.sock_type = SOCK_STREAM;
    }
  else if (strcasecmp(self->super.transport, "tls") == 0)
    {
      static gboolean msg_tls_source_port_warning = FALSE;

      g_assert(self->super.flags & AFSOCKET_SYSLOG_PROTOCOL);
      if (!self->bind_port)
        {
          /* NOTE: this version number change has happened in a different
           * major version in OSE vs. PE, thus the update behaviour must
           * be triggered differently.  In OSE it needs to be triggered
           * when the config version has changed to 3.3, in PE when 3.2.
           *
           * This is unfortunate, the only luck we have to be less
           * confusing is that syslog() driver was seldom used.
           *
           */

          if (cfg_is_config_version_older(cfg, 0x0303))
            {
              if (!msg_tls_source_port_warning)
                {
                  msg_warning("WARNING: Default port for syslog(transport(tls))  has changed from 601 to 6514 in syslog-ng 3.3, please update your configuration",
                              evt_tag_str("id", self->super.super.super.id),
                              NULL);
                  msg_tls_source_port_warning = TRUE;
                }
              default_bind_port = "601";
            }
          else
            default_bind_port = "6514";
        }
      self->super.flags |= AFSOCKET_REQUIRE_TLS;
      self->super.sock_type = SOCK_STREAM;
      self->super.logproto_name = "framed";
    }
  else
    {
      self->super.logproto_name = self->super.transport;
      self->super.sock_type = SOCK_STREAM;
    }

  if (!self->super.sock_protocol)
    {
      if (self->super.sock_type == SOCK_STREAM)
        self->super.sock_protocol = IPPROTO_TCP;
      else
        self->super.sock_protocol = IPPROTO_UDP;
    }

  ipproto_ent = getprotobynumber(self->super.sock_protocol);
  afinet_set_port(self->super.bind_addr, self->bind_port ? : default_bind_port,
                  ipproto_ent ? ipproto_ent->p_name
                              : (self->super.sock_type == SOCK_STREAM) ? "tcp" : "udp");
  if (!resolve_hostname(&self->super.bind_addr, self->bind_ip ? : default_bind_ip))
    return FALSE;

#if BUILD_WITH_SSL
  if (self->super.flags & AFSOCKET_REQUIRE_TLS && !self->super.tls_context)
    {
      msg_error("transport(tls) was specified, but tls() options missing",
                evt_tag_str("id", self->super.super.super.id),
                NULL);
      return FALSE;
    }
#endif

  return TRUE;
}

void
afinet_sd_free(LogPipe *s)
{
  AFInetSourceDriver *self = (AFInetSourceDriver *) s;

  g_free(self->bind_ip);
  g_free(self->bind_port);
  afsocket_sd_free(s);
}

LogDriver *
afinet_sd_new(gint af, gint sock_type, guint flags)
{
  AFInetSourceDriver *self = g_new0(AFInetSourceDriver, 1);

  afsocket_sd_init_instance(&self->super, &self->sock_options.super, af, sock_type, flags);
  self->super.super.super.super.free_fn = afinet_sd_free;

  self->super.setup_socket = afinet_sd_setup_socket;
  self->super.apply_transport = afinet_sd_apply_transport;
  return &self->super.super.super;
}
