/*
 * Copyright (c) 2025 Balazs Scheidler <balazs.scheidler@axoflow.com>
 * Copyright (c) 2025 Axoflow
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

#include "transport/transport-factory-haproxy.h"
#include "transport/transport-haproxy.h"

typedef struct _LogTransportFactoryHAProxy
{
  LogTransportFactory super;
  LogTransportIndex base;
  LogTransportIndex switch_to;
} LogTransportFactoryHAProxy;

static LogTransport *
_construct_transport(const LogTransportFactory *s, LogTransportStack *stack)
{
  LogTransportFactoryHAProxy *self = (LogTransportFactoryHAProxy *) s;

  return log_transport_haproxy_new(self->base == LOG_TRANSPORT_NONE ? stack->active_transport : self->base,
                                   self->switch_to == LOG_TRANSPORT_NONE ? stack->active_transport : self->switch_to);
}

LogTransportFactory *
transport_factory_haproxy_new(LogTransportIndex base, LogTransportIndex switch_to)
{
  LogTransportFactoryHAProxy *self = g_new0(LogTransportFactoryHAProxy, 1);

  log_transport_factory_init_instance(&self->super, LOG_TRANSPORT_HAPROXY);
  self->super.construct_transport = _construct_transport;
  self->base = base;
  self->switch_to = switch_to;
  return &self->super;
}
