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
#include "transport-mapper-inet.h"
#include "afinet.h"
#include "cfg.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "transport/transport-tls.h"
#include "transport/multitransport.h"
#include "transport/transport-factory-tls.h"
#include "transport/transport-factory-socket.h"
#include "transport/transport-udp-socket.h"
#include "secret-storage/secret-storage.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#define UDP_PORT                  514
#define TCP_PORT                  514
#define NETWORK_PORT              514
#define SYSLOG_TRANSPORT_UDP_PORT 514
#define SYSLOG_TRANSPORT_TCP_PORT 601
#define SYSLOG_TRANSPORT_TLS_PORT 6514

static inline gboolean
_is_tls_required(TransportMapperInet *self)
{
  return self->require_tls || (self->tls_context && self->require_tls_when_has_tls_context);
}

static inline gboolean
_is_tls_allowed(TransportMapperInet *self)
{
  return self->require_tls || self->allow_tls || self->require_tls_when_has_tls_context;
}

static gboolean
transport_mapper_inet_validate_tls_options(TransportMapperInet *self)
{
  if (!self->tls_context && _is_tls_required(self))
    {
      msg_error("transport(tls) was specified, but tls() options missing");
      return FALSE;
    }
  else if (self->tls_context && !_is_tls_allowed(self))
    {
      msg_error("tls() options specified for a transport that doesn't allow TLS encryption",
                evt_tag_str("transport", self->super.transport));
      return FALSE;
    }
  return TRUE;
}

static gboolean
transport_mapper_inet_apply_transport_method(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  if (!transport_mapper_apply_transport_method(s, cfg))
    return FALSE;

  return transport_mapper_inet_validate_tls_options(self);
}

static LogTransport *
_construct_multitransport_with_tls_factory(TransportMapperInet *self, gint fd)
{
  TransportFactory *default_factory = transport_factory_tls_new(self->tls_context, self->tls_verifier, self->flags);
  return multitransport_new(default_factory, fd);
}

static LogTransport *
_construct_tls_or_multi_transport(TransportMapperInet *self, gboolean create_multi_transport, gint fd)
{
  if (create_multi_transport)
    return _construct_multitransport_with_tls_factory(self, fd);

  TLSSession *tls_session = tls_context_setup_session(self->tls_context);
  if (!tls_session)
    return NULL;

  tls_session_configure_allow_compress(tls_session, self->flags & TMI_ALLOW_COMPRESS);
  tls_session_set_verifier(tls_session, self->tls_verifier);

  return log_transport_tls_new(tls_session, fd);
}

static LogTransport *
_construct_multitransport_with_plain_tcp_factory(TransportMapperInet *self, gint fd)
{
  TransportFactory *default_factory = transport_factory_socket_new(self->super.sock_type);
  return multitransport_new(default_factory, fd);
}

static LogTransport *
_construct_multitransport_with_plain_and_tls_factories(TransportMapperInet *self, gint fd)
{
  LogTransport *transport = _construct_multitransport_with_plain_tcp_factory(self, fd);

  TransportFactory *tls_factory = transport_factory_tls_new(self->tls_context, self->tls_verifier, self->flags);
  multitransport_add_factory((MultiTransport *)transport, tls_factory);

  return transport;
}

static LogTransport *
_construct_plain_tcp_or_multi_transport(TransportMapperInet *self, gboolean create_multi_transport, gint fd)
{
  if (create_multi_transport)
    return _construct_multitransport_with_plain_tcp_factory(self, fd);

  if (self->super.sock_type == SOCK_DGRAM)
    return log_transport_udp_socket_new(fd);
  else
    return log_transport_stream_socket_new(fd);
}

static LogTransport *
transport_mapper_inet_construct_log_transport(TransportMapper *s, gint fd)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  gboolean proxy_should_switch_transport = FALSE;
  LogTransport *transport = NULL;

  if (self->tls_context && _is_tls_required(self))
    {
      transport = _construct_tls_or_multi_transport(self, self->super.create_multitransport, fd);
    }
  else if (self->tls_context)
    {
      proxy_should_switch_transport = TRUE;
      transport = _construct_multitransport_with_plain_and_tls_factories(self, fd);
    }
  else
    {
      transport = _construct_plain_tcp_or_multi_transport(self, self->super.create_multitransport, fd);
    }

  if (self->proxied)
    log_transport_socket_proxy_new(transport, proxy_should_switch_transport);

  return transport;
}

static gboolean
transport_mapper_inet_init(TransportMapper *s)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  if (self->tls_context && (tls_context_setup_context(self->tls_context) != TLS_CONTEXT_SETUP_OK))
    return FALSE;

  return TRUE;
}

typedef struct _call_finalize_init_args
{
  TransportMapperInet *transport_mapper_inet;
  TransportMapperAsyncInitCB func;
  gpointer func_args;
} call_finalize_init_args;

static void
_call_finalize_init(Secret *secret, gpointer user_data)
{
  call_finalize_init_args *args = user_data;
  TransportMapperInet *self = args->transport_mapper_inet;

  if (!self)
    return;

  TLSContextSetupResult r = tls_context_setup_context(self->tls_context);
  const gchar *key = tls_context_get_key_file(self->tls_context);

  switch (r)
    {
    case TLS_CONTEXT_SETUP_ERROR:
    {
      msg_error("Error setting up TLS context",
                evt_tag_str("keyfile", key));
      secret_storage_update_status(key, SECRET_STORAGE_STATUS_FAILED);
      return;
    }
    case TLS_CONTEXT_SETUP_BAD_PASSWORD:
    {
      msg_error("Invalid password, error setting up TLS context",
                evt_tag_str("keyfile", key));

      if (!secret_storage_subscribe_for_key(key, _call_finalize_init, args))
        msg_error("Failed to subscribe for key",
                  evt_tag_str("keyfile", key));
      else
        msg_debug("Re-subscribe for key",
                  evt_tag_str("keyfile", key));

      secret_storage_update_status(key, SECRET_STORAGE_STATUS_INVALID_PASSWORD);

      return;
    }
    default:
      secret_storage_update_status(key, SECRET_STORAGE_SUCCESS);
      if (!args->func(args->func_args))
        {
          msg_error("Error finalize initialization",
                    evt_tag_str("keyfile", key));
        }
    }
}

static gboolean
transport_mapper_inet_async_init(TransportMapper *s, TransportMapperAsyncInitCB func, gpointer func_args)
{
  TransportMapperInet *self = (TransportMapperInet *)s;

  if (!self->tls_context)
    return func(func_args);

  TLSContextSetupResult tls_ctx_setup_res = tls_context_setup_context(self->tls_context);

  const gchar *key = tls_context_get_key_file(self->tls_context);

  if (tls_ctx_setup_res == TLS_CONTEXT_SETUP_OK)
    {
      if (key && secret_storage_contains_key(key))
        secret_storage_update_status(key, SECRET_STORAGE_SUCCESS);
      return func(func_args);
    }

  if (tls_ctx_setup_res == TLS_CONTEXT_SETUP_BAD_PASSWORD)
    {
      msg_error("Error setting up TLS context",
                evt_tag_str("keyfile", key));
      call_finalize_init_args *args = g_new0(call_finalize_init_args, 1);
      args->transport_mapper_inet = self;
      args->func = func;
      args->func_args = func_args;
      self->secret_store_cb_data = args;
      gboolean subscribe_res = secret_storage_subscribe_for_key(key, _call_finalize_init, args);
      if (subscribe_res)
        msg_info("Waiting for password",
                 evt_tag_str("keyfile", key));
      else
        msg_error("Failed to subscribe for key",
                  evt_tag_str("keyfile", key));
      return subscribe_res;
    }

  return FALSE;
}

void
transport_mapper_inet_free_method(TransportMapper *s)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  if (self->secret_store_cb_data)
    {
      const gchar *key = tls_context_get_key_file(self->tls_context);
      secret_storage_unsubscribe(key, _call_finalize_init, self->secret_store_cb_data);
      g_free(self->secret_store_cb_data);
    }

  if (self->tls_verifier)
    tls_verifier_unref(self->tls_verifier);
  if (self->tls_context)
    tls_context_unref(self->tls_context);

  transport_mapper_free_method(s);
}

void
transport_mapper_inet_init_instance(TransportMapperInet *self, const gchar *transport)
{
  transport_mapper_init_instance(&self->super, transport);
  self->super.apply_transport = transport_mapper_inet_apply_transport_method;
  self->super.construct_log_transport = transport_mapper_inet_construct_log_transport;
  self->super.init = transport_mapper_inet_init;
  self->super.async_init = transport_mapper_inet_async_init;
  self->super.free_fn = transport_mapper_inet_free_method;
  self->super.address_family = AF_INET;
}

TransportMapperInet *
transport_mapper_inet_new_instance(const gchar *transport)
{
  TransportMapperInet *self = g_new0(TransportMapperInet, 1);

  transport_mapper_inet_init_instance(self, transport);
  return self;
}

static gboolean
transport_mapper_tcp_apply_transport(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  if (!transport_mapper_inet_apply_transport_method(s, cfg))
    return FALSE;

  if (self->tls_context)
    self->super.transport_name = g_strdup("rfc3164+tls");
  else
    self->super.transport_name = g_strdup("rfc3164+tcp");

  return TRUE;
}

TransportMapper *
transport_mapper_tcp_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.apply_transport = transport_mapper_tcp_apply_transport;
  self->super.sock_type = SOCK_STREAM;
  self->super.sock_proto = IPPROTO_TCP;
  self->super.logproto = "text";
  self->super.stats_source = stats_register_type("tcp");
  self->server_port = TCP_PORT;
  self->require_tls_when_has_tls_context = TRUE;
  return &self->super;
}

TransportMapper *
transport_mapper_tcp6_new(void)
{
  TransportMapper *self = transport_mapper_tcp_new();

  self->address_family = AF_INET6;
  self->stats_source = stats_register_type("tcp6");
  return self;
}

TransportMapper *
transport_mapper_udp_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("udp");

  self->super.transport_name = g_strdup("rfc3164+udp");
  self->super.sock_type = SOCK_DGRAM;
  self->super.sock_proto = IPPROTO_UDP;
  self->super.logproto = "dgram";
  self->super.stats_source = stats_register_type("udp");
  self->server_port = UDP_PORT;
  return &self->super;
}

TransportMapper *
transport_mapper_udp6_new(void)
{
  TransportMapper *self = transport_mapper_udp_new();

  self->address_family = AF_INET6;
  self->stats_source = stats_register_type("udp6");
  return self;
}

static gboolean
transport_mapper_network_apply_transport(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;
  const gchar *transport;

  /* determine default port, apply transport setting to afsocket flags */

  if (!transport_mapper_apply_transport_method(s, cfg))
    return FALSE;

  transport = self->super.transport;
  self->server_port = NETWORK_PORT;
  if (strcasecmp(transport, "udp") == 0)
    {
      self->super.logproto = "dgram";
      self->super.sock_type = SOCK_DGRAM;
      self->super.sock_proto = IPPROTO_UDP;
      self->super.transport_name = g_strdup("rfc3164+udp");
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->super.transport_name = g_strdup("rfc3164+tcp");
    }
  else if (strcasecmp(transport, "proxied-tcp") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->super.transport_name = g_strdup("rfc3164+proxied-tcp");
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->require_tls = TRUE;
      self->super.transport_name = g_strdup("rfc3164+tls");
    }
  else if (strcasecmp(transport, "proxied-tls") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->require_tls = TRUE;
      self->super.transport_name = g_strdup("rfc3164+proxied-tls");
    }
  else if (strcasecmp(transport, "proxied-tls-passthrough") == 0)
    {
      self->super.logproto = "text";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->allow_tls = TRUE;
      self->super.transport_name = g_strdup("rfc3164+proxied-tls-passthrough");
    }
  else if (strcasecmp(transport, "http") == 0)
    {
      self->super.logproto = "http";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->super.transport_name = g_strdup("http");
    }
  else
    {
      self->super.logproto = self->super.transport;
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      /* FIXME: look up port/protocol from the logproto */
      self->server_port = TCP_PORT;
      self->allow_tls = TRUE;
      self->super.transport_name = g_strdup_printf("rfc3164+%s", self->super.transport);
    }

  g_assert(self->server_port != 0);

  if (!transport_mapper_inet_validate_tls_options(self))
    return FALSE;

  return TRUE;
}

TransportMapper *
transport_mapper_network_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.apply_transport = transport_mapper_network_apply_transport;
  self->super.stats_source = stats_register_type("network");
  return &self->super;
}

static gboolean
transport_mapper_syslog_apply_transport(TransportMapper *s, GlobalConfig *cfg)
{
  TransportMapperInet *self = (TransportMapperInet *) s;
  const gchar *transport = self->super.transport;

  /* determine default port, apply transport setting to afsocket flags */

  if (!transport_mapper_apply_transport_method(s, cfg))
    return FALSE;

  if (strcasecmp(transport, "udp") == 0)
    {
      if (cfg_is_config_version_older(cfg, VERSION_VALUE_3_3))
        {
          self->server_port_change_warning = "WARNING: Default port for syslog(transport(udp)) has changed from 601 to 514 in "
                                             VERSION_3_3 ", please update your configuration";
          self->server_port = 601;
        }
      else
        self->server_port = SYSLOG_TRANSPORT_UDP_PORT;

      self->super.logproto = "dgram";
      self->super.sock_type = SOCK_DGRAM;
      self->super.sock_proto = IPPROTO_UDP;
      self->super.transport_name = g_strdup("rfc5426");
    }
  else if (strcasecmp(transport, "tcp") == 0)
    {
      self->server_port = SYSLOG_TRANSPORT_TCP_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->super.transport_name = g_strdup("rfc6587");
    }
  else if (strcasecmp(transport, "proxied-tcp") == 0)
    {
      self->server_port = SYSLOG_TRANSPORT_TCP_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->super.transport_name = g_strdup("rfc6587+proxied-tcp");
    }
  else if (strcasecmp(transport, "tls") == 0)
    {
      if (cfg_is_config_version_older(cfg, VERSION_VALUE_3_3))
        {
          self->server_port_change_warning = "WARNING: Default port for syslog(transport(tls))  has changed from 601 to 6514 in "
                                             VERSION_3_3 ", please update your configuration";
          self->server_port = 601;
        }
      else
        self->server_port = SYSLOG_TRANSPORT_TLS_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->require_tls = TRUE;
      self->super.transport_name = g_strdup("rfc5425");
    }
  else if (strcasecmp(transport, "proxied-tls") == 0)
    {
      self->server_port = SYSLOG_TRANSPORT_TCP_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->require_tls = TRUE;
      self->super.transport_name = g_strdup("rfc5424+proxied-tls");
    }
  else if (strcasecmp(transport, "proxied-tls-passthrough") == 0)
    {
      self->server_port = SYSLOG_TRANSPORT_TCP_PORT;
      self->super.logproto = "framed";
      self->super.sock_type = SOCK_STREAM;
      self->super.sock_proto = IPPROTO_TCP;
      self->proxied = TRUE;
      self->allow_tls = TRUE;
      self->super.transport_name = g_strdup("rfc5424+proxied-tls-passthrough");
    }
  else
    {
      self->super.logproto = self->super.transport;
      self->super.sock_type = SOCK_STREAM;
      /* FIXME: look up port/protocol from the logproto */
      self->server_port = 514;
      self->super.sock_proto = IPPROTO_TCP;
      self->allow_tls = TRUE;
      self->super.transport_name = g_strdup_printf("rfc5424+%s", self->super.transport);
    }
  g_assert(self->server_port != 0);

  if (!transport_mapper_inet_validate_tls_options(self))
    return FALSE;

  return TRUE;
}

TransportMapper *
transport_mapper_syslog_new(void)
{
  TransportMapperInet *self = transport_mapper_inet_new_instance("tcp");

  self->super.apply_transport = transport_mapper_syslog_apply_transport;
  self->super.stats_source = stats_register_type("syslog");
  return &self->super;
}
