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

#include "transport/transport-factory-registry.h"

struct _TransportFactoryRegistry
{
  /*
   * hash_table<TransportFactoryId, TransportFactory *>
   * */
  GHashTable *registry;
};

static void
_transport_factory_destroy_notify(gpointer s)
{
  TransportFactory *self = (TransportFactory *)s;
  transport_factory_free(self);
}

TransportFactoryRegistry *transport_factory_registry_new(void)
{
  TransportFactoryRegistry *instance = g_new0(TransportFactoryRegistry, 1);

  instance->registry = g_hash_table_new_full((GHashFunc)transport_factory_id_hash,
                                             (GEqualFunc)transport_factory_id_equal,
                                             NULL,_transport_factory_destroy_notify);

  return instance;
}

void
transport_factory_registry_free(TransportFactoryRegistry *self)
{
  g_hash_table_unref(self->registry);
  g_free(self);
}

void
transport_factory_registry_add(TransportFactoryRegistry *self, TransportFactory *factory)
{
  const TransportFactoryId *id = transport_factory_get_id(factory);
  g_hash_table_insert(self->registry, (TransportFactoryId *)id, factory);
}

const TransportFactory *
transport_factory_registry_lookup(TransportFactoryRegistry *self, const TransportFactoryId *id)
{
  return g_hash_table_lookup(self->registry, id);
}
