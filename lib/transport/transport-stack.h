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

/* LogTransportFactory is an interface for representing
 * concrete LogTransportFactory instances
 * Each LogTransportFactory has
 *  - a reference to a unique id
 *  - a construct method for creating new LogTransport instances
 *  - a destroy method for releasing resources that are needed for construct()
 */
typedef struct _LogTransportFactory LogTransportFactory;

struct _LogTransportFactory
{
  const gchar *id;
  LogTransport *(*construct_transport)(const LogTransportFactory *self, gint fd);
  void (*free_fn)(LogTransportFactory *self);
};

static inline LogTransport *
log_transport_factory_construct_transport(const LogTransportFactory *self, gint fd)
{
  g_assert(self->construct_transport);

  LogTransport *transport = self->construct_transport(self, fd);

  return transport;
}

static inline void
log_transport_factory_free(LogTransportFactory *self)
{
  if (self->free_fn)
    self->free_fn(self);
  g_free(self);
}

static inline const gchar *
log_transport_factory_get_id(const LogTransportFactory *self)
{
  /* each concrete LogTransportFactory has to have an id
   */
  g_assert(self->id);
  return self->id;
}

typedef struct _LogTransportStack LogTransportStack;

struct _LogTransportStack
{
  LogTransport super;
  GHashTable *registry;
  LogTransport *active_transport;
  const LogTransportFactory *active_transport_factory;
};

LogTransport *log_transport_stack_new(LogTransportFactory *default_transport_factory, gint fd);
void log_transport_stack_add_factory(LogTransportStack *self, LogTransportFactory *);
gboolean log_transport_stack_switch(LogTransportStack *self, const gchar *id);
gboolean log_transport_stack_contains_factory(LogTransportStack *self, const gchar *id);

#endif
