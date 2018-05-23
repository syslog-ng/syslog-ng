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

#ifndef MULTITRANSPORT_H_INCLUDED
#define MULTITRANSPORT_H_INCLUDED

#include "transport/logtransport.h"
#include "transport/transport-factory.h"
#include "transport/transport-factory-registry.h"

typedef struct _MultiTransport MultiTransport;

struct _MultiTransport
{
  LogTransport super;
  TransportFactoryRegistry *registry;
  LogTransport *active_transport;
  const TransportFactory *active_transport_factory;
};

LogTransport *multitransport_new(TransportFactory *default_transport_factory, gint fd);
void multitransport_add_factory(MultiTransport *, TransportFactory *);
gboolean multitransport_switch(MultiTransport *, const TransportFactoryId *);

#endif

