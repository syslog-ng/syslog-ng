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
  LOG_TRANSPORT_INITIAL,
  LOG_TRANSPORT_FD = LOG_TRANSPORT_INITIAL,
  LOG_TRANSPORT_SOCKET = LOG_TRANSPORT_INITIAL,
  LOG_TRANSPORT_TLS,
  LOG_TRANSPORT_HAPROXY,
  LOG_TRANSPORT_GZIP,
  LOG_TRANSPORT_ZLIB,
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
  LogTransportAuxData aux_data;
};

static inline LogTransportFactory *
log_transport_stack_get_transport_factory(LogTransportStack *self, gint index)
{
  g_assert(index < LOG_TRANSPORT__MAX);

  return self->transport_factories[index];
}

static inline LogTransport *
log_transport_stack_get_transport(LogTransportStack *self, gint index)
{
  g_assert(index < LOG_TRANSPORT__MAX);

  return self->transports[index];
}

static inline LogTransport *
log_transport_stack_get_or_create_transport(LogTransportStack *self, gint index)
{
  LogTransport *transport = log_transport_stack_get_transport(self, index);

  if (transport)
    return transport;

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
  // TODO - Change it to log_transport_stack_get_transport after checking call sites
  return log_transport_stack_get_or_create_transport(self, self->active_transport);
}

static inline gboolean
log_transport_stack_poll_prepare(LogTransportStack *self, GIOCondition *cond)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  return log_transport_poll_prepare(transport, cond);
}

static inline LogTransportIOCond
log_transport_stack_get_io_requirement(LogTransportStack *self)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  return log_transport_get_io_requirement(transport);
}

static inline gssize
log_transport_stack_write(LogTransportStack *self, const gpointer buf, gsize count)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  return log_transport_write(transport, buf, count);
}

static inline gssize
log_transport_stack_writev(LogTransportStack *self, struct iovec *iov, gint iov_count)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  return log_transport_writev(transport, iov, iov_count);
}

static inline gssize
log_transport_stack_read(LogTransportStack *self, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  log_transport_aux_data_copy(aux, &self->aux_data);
  return log_transport_read(transport, buf, count, aux);
}

static inline gssize
log_transport_stack_read_ahead(LogTransportStack *self, gpointer buf, gsize count, gboolean *moved_forward)
{
  LogTransport *transport = log_transport_stack_get_active(self);
  return log_transport_read_ahead(transport, buf, count, moved_forward);
}

void log_transport_stack_add_factory(LogTransportStack *self, LogTransportFactory *);
void log_transport_stack_add_transport(LogTransportStack *self, gint index, LogTransport *);
gboolean log_transport_stack_switch(LogTransportStack *self, gint index);
void log_transport_stack_move(LogTransportStack *self, LogTransportStack *other);
void log_transport_stack_shutdown(LogTransportStack *self);

void log_transport_stack_init(LogTransportStack *self, LogTransport *initial_transport);
void log_transport_stack_deinit(LogTransportStack *self);

#endif
