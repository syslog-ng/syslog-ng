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

#include "transport/transport-factory-id.h"
#include "mainloop.h"

struct _TransportFactoryId
{
  gchar *transport_name;
  gchar *uniq_id;
};

static GList *transport_factory_ids;

void transport_factory_id_global_init(void)
{
}

static inline void _free(gpointer s)
{
  transport_factory_id_free((TransportFactoryId *)s);
}

void transport_factory_id_global_deinit(void)
{
  g_list_free_full(transport_factory_ids, _free);
  transport_factory_ids = NULL;
}

void
transport_factory_id_register(TransportFactoryId *id)
{
  main_loop_assert_main_thread();
  transport_factory_ids = g_list_append(transport_factory_ids, id);
}

static gpointer
_clone(gconstpointer s)
{
  TransportFactoryId *id = (TransportFactoryId *)s;
  TransportFactoryId *cloned = transport_factory_id_new(g_strdup(id->transport_name),
                                                        g_strdup(id->uniq_id));
  return cloned;
}

TransportFactoryId *transport_factory_id_clone(const TransportFactoryId *id)
{
  return (TransportFactoryId *)_clone((gconstpointer) id);
}

static gpointer
_copy_func(gconstpointer src, gpointer data)
{
  return _clone(src);
}

GList *
transport_factory_id_clone_registered_ids(void)
{
  return g_list_copy_deep(transport_factory_ids, _copy_func, NULL);
}

void
transport_factory_id_free(TransportFactoryId *id)
{
  g_free(id->transport_name);
  g_free(id->uniq_id);
  g_free(id);
}

guint
transport_factory_id_hash(gconstpointer key)
{
  TransportFactoryId *id = (TransportFactoryId *)key;
  return g_str_hash(id->uniq_id);
}

gboolean
transport_factory_id_equal(const TransportFactoryId *id1, const TransportFactoryId *id2)
{
  return g_str_equal(id1->uniq_id, id2->uniq_id);
}

const gchar *
transport_factory_id_get_transport_name(const TransportFactoryId *id)
{
  return id->transport_name;
}

TransportFactoryId *
transport_factory_id_new(const gchar *transport_name, const gchar *uniq_id)
{
  TransportFactoryId *id = g_new0(TransportFactoryId, 1);

  id->transport_name = g_strdup(transport_name);
  id->uniq_id = g_strdup(uniq_id);

  return id;
}
