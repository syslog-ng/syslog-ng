/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) Laszlo Budai <laszlo.budai@balabit.com>
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

#ifndef TRANSPORT_FACTORY_REGISTRY_H_INCLUDED
#define TRANSPORT_FACTORY_REGISTRY_H_INCLUDED

#include "syslog-ng.h"
#include "transport/transport-factory.h"
#include "transport/transport-factory-id.h"

typedef struct _TransportFactoryRegistry TransportFactoryRegistry;

TransportFactoryRegistry *transport_factory_registry_new(void);
void transport_factory_registry_free(TransportFactoryRegistry *self);

gboolean transport_factory_registry_add(TransportFactoryRegistry *self, TransportFactory *factory);
const TransportFactory *transport_factory_registry_lookup(TransportFactoryRegistry *self, const TransportFactoryId *id);

#endif

