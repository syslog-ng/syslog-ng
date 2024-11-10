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

#ifndef TRANSPORT_STACK_H_INCLUDED
#define TRANSPORT_STACK_H_INCLUDED

#include "transport/logtransport.h"

typedef struct _LogTransportFactory LogTransportFactory;

typedef enum
{
  /* this is a special index for simple cases where we only use a single
   * LogTransport which never changes */
  LOG_TRANSPORT_INITIAL,
  LOG_TRANSPORT_SOCKET,
  LOG_TRANSPORT_TLS,
  LOG_TRANSPORT_HAPROXY,
  LOG_TRANSPORT_GZIP,
  LOG_TRANSPORT_NONE,
  LOG_TRANSPORT__MAX = LOG_TRANSPORT_NONE,
} LogTransportIndex;

struct _LogTransportFactory
{
  gint index;
  LogTransport *(*construct_transport)(const LogTransportFactory *self, LogTransportStack *stack);
  void (*free_fn)(LogTransportFactory *self);
};

static inline LogTransport *
log_transport_factory_construct_transport(const LogTransportFactory *self, LogTransportStack *stack)
{
  g_assert(self->construct_transport);

  LogTransport *transport = self->construct_transport(self, stack);

  return transport;
}

static inline void
log_transport_factory_free(LogTransportFactory *self)
{
  if (self->free_fn)
    self->free_fn(self);
  g_free(self);
}

void log_transport_factory_init_instance(LogTransportFactory *self, LogTransportIndex index);


/* LogTransportStack
 *
 * This is a struct embedded into a LogProto instance and represents a
 * combination of LogTransport instances that somehow work together.
 * LogTransportStack allows "switching" between LogTransport instances so we
 * can implement changes on the transport layer mid stream.  Such a change
 * would be for example the switch to the TLS transport after a STARTTLS.
 *
 * It is also possible to stack log transports, e.g.  LogTransportAdapter
 * would add some further processing on top of another.  A use case here is
 * the HAProxy implementation, which reads the proxy header from
 * LogTransportSocket, processes it and then allows the LogProto
 * implementation to interact with the peer.
 *
 * LogTransportStack ensures that all LogTransport instances are freed at
 * the same time, so there's no need to keep references around.  It also
 * manages the closing of the underlying file descriptor.
 *
 * Right now, transports are identified using the LOG_TRANSPORT_XXXX enum,
 * meaning that any new "layer" we can potentially push to the transport
 * stack will need a new element in the enumeration.  Although this is a bit
 * of a layering violation and prevents new LogTransport implementations
 * coming from plugins, it is only an easy to change implementation detail,
 * which is perfectly fine with all transports provided by the core itself.
 *
 * A layer in the stack is either instantiated by the caller and pushed to
 * the stack _OR_ it is instantiated automatically by LogTransportStack
 * using the LogTransportFactory interface.
 */
struct _LogTransportStack
{
  gint active_transport;
  gint fd;
  LogTransport *transports[LOG_TRANSPORT__MAX];
  LogTransportFactory *transport_factories[LOG_TRANSPORT__MAX];
};

static inline LogTransport *
log_transport_stack_get_transport(LogTransportStack *self, gint index)
{
  g_assert(index < LOG_TRANSPORT__MAX);

  if (self->transports[index])
    return self->transports[index];

  if (self->transport_factories[index])
    {
      self->transports[index] = log_transport_factory_construct_transport(self->transport_factories[index], self);
      log_transport_assign_to_stack(self->transports[index], self);
      return self->transports[index];
    }
  return NULL;
}

static inline LogTransport *
log_transport_stack_get_active(LogTransportStack *self)
{
  return log_transport_stack_get_transport(self, self->active_transport);
}

void log_transport_stack_add_factory(LogTransportStack *self, LogTransportFactory *);
void log_transport_stack_add_transport(LogTransportStack *self, gint index, LogTransport *);
gboolean log_transport_stack_switch(LogTransportStack *self, gint index);
void log_transport_stack_move(LogTransportStack *self, LogTransportStack *other);

void log_transport_stack_init(LogTransportStack *self, LogTransport *initial_transport);
void log_transport_stack_deinit(LogTransportStack *self);

#endif
