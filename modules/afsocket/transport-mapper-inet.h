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
#ifndef TRANSPORT_MAPPER_INET_H_INCLUDED
#define TRANSPORT_MAPPER_INET_H_INCLUDED

#include "transport-mapper.h"
#include "transport/tls-context.h"

typedef struct _TransportMapperInet
{
  TransportMapper super;

  gint server_port;
  const gchar *server_port_change_warning;
  /* tls() options are required */
  gboolean require_tls_configuration;
  /* tls() options are optional, but are permitted */
  gboolean allow_tls_configuration;

  /* tls() options may be specified, but it is up to the transport/logproto
   * plugin to start TLS.
   *
   * If this is TRUE, TLS encapsulation is started by either the
   * LogTransport or the LogProtoServer instances with an explicit call to
   * log_transport_stack_switch(LOG_TRANSPORT_TLS).  For example, this
   * mechanism is used by LogProtoAutoServer (when TLS is detected), by
   * LogTransportHAProxy or any potential LogProto implementations that have
   * an explicit STARTTLS command (e.g. ALTP, RLTP).
   *
   * If this is FALSE, TLS will be started before LogProtoServer has any
   * chance to read data,
   */

  gboolean delegate_tls_start_to_logproto;

  /* HAProxy v1 or v2 protocol is to be used */
  gboolean proxied;
  /* switch to TLS after plaintext haproxy negotiation */
  gboolean proxied_passthrough;
  TLSContext *tls_context;
  TLSVerifier *tls_verifier;
  gpointer secret_store_cb_data;
} TransportMapperInet;

static inline gint
transport_mapper_inet_get_server_port(const TransportMapper *self)
{
  return ((TransportMapperInet *) self)->server_port;
}

static inline void
transport_mapper_inet_set_server_port(TransportMapper *self, gint server_port)
{
  ((TransportMapperInet *) self)->server_port = server_port;
}

static inline const gchar *
transport_mapper_inet_get_port_change_warning(TransportMapper *s)
{
  TransportMapperInet *self = (TransportMapperInet *) s;

  return self->server_port_change_warning;
}

static inline void
transport_mapper_inet_set_tls_context(TransportMapperInet *self, TLSContext *tls_context)
{
  self->tls_context = tls_context;
}

static inline void
transport_mapper_inet_set_tls_verifier(TransportMapperInet *self, TLSVerifier *tls_verifier)
{
  tls_verifier_unref(self->tls_verifier);
  self->tls_verifier = tls_verifier;
}

void transport_mapper_inet_init_instance(TransportMapperInet *self, const gchar *transport);
TransportMapper *transport_mapper_tcp_new(void);
TransportMapper *transport_mapper_tcp6_new(void);
TransportMapper *transport_mapper_udp_new(void);
TransportMapper *transport_mapper_udp6_new(void);
TransportMapper *transport_mapper_network_new(void);
TransportMapper *transport_mapper_syslog_new(void);

#endif
