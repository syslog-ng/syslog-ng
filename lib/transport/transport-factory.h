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

#ifndef TRANSPORT_FACTORY_H_INCLUDED
#define TRANSPORT_FACTORY_H_INCLUDED

#include "transport/logtransport.h"
#include "transport/transport-factory-id.h"

/* TransportFactory is an interface for representing
 * concrete TransportFactory instances
 * Each TransportFactory has
 *  - a reference to a unique id
 *  - a construct method for creating new LogTransport instances
 *  - a destroy method for releasing resources that are needed for construct()
 */
typedef struct _TransportFactory TransportFactory;

struct _TransportFactory
{
  const TransportFactoryId *id;
  LogTransport *(*construct_transport)(const TransportFactory *self, gint fd);
  void (*free_fn)(TransportFactory *self);
};

static inline LogTransport
*transport_factory_construct_transport(const TransportFactory *self, gint fd)
{
  g_assert(self->construct_transport);

  LogTransport *transport = self->construct_transport(self, fd);
  transport->name = transport_factory_id_get_transport_name(self->id);

  return transport;
}

static inline void
transport_factory_free(TransportFactory *self)
{
  if (self->free_fn)
    self->free_fn(self);
  g_free(self);
}

static inline const
TransportFactoryId *transport_factory_get_id(const TransportFactory *self)
{
  /* each concrete TransportFactory has to have an id
   */
  g_assert(self->id);
  return self->id;
}

#endif
