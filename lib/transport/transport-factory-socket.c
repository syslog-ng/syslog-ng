/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 2018 Laszlo Budai <laszlo.budai@balabit.com>
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

#include "transport/transport-factory-socket.h"
#include "transport/transport-socket.h"
#include "transport/transport-udp-socket.h"

static LogTransport *
_construct_transport_dgram(const LogTransportFactory *s, gint fd)
{
  return log_transport_udp_socket_new(fd);
}

static LogTransport *
_construct_transport_stream(const LogTransportFactory *s, gint fd)
{
  return log_transport_stream_socket_new(fd);
}

LogTransportFactory *
log_transport_factory_socket_new(int sock_type)
{
  LogTransportFactorySocket *self = g_new0(LogTransportFactorySocket, 1);

  if (sock_type == SOCK_DGRAM)
    self->super.construct_transport = _construct_transport_dgram;
  else
    self->super.construct_transport = _construct_transport_stream;

  self->super.id = TRANSPORT_FACTORY_SOCKET_ID;

  return &self->super;
}

