/*
 * Copyright (c) 2023 One Identity LLC.
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

#include "transport/transport-factory-proxied-socket.h"
#include "transport/transport-socket-proxy.h"

DEFINE_TRANSPORT_FACTORY_ID_FUN("proxied-socket", transport_factory_proxied_socket_id);

static LogTransport *
_construct_transport_proxied_stream(const TransportFactory *s, gint fd)
{
  return log_transport_proxied_stream_socket_new(fd);
}

TransportFactory *
transport_factory_proxied_socket_new(int sock_type)
{
  TransportFactoryProxiedSocket *self = g_new0(TransportFactoryProxiedSocket, 1);

  self->super.construct_transport = _construct_transport_proxied_stream;
  self->super.id = transport_factory_proxied_socket_id();

  return &self->super;
}
