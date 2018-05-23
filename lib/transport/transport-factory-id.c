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

static GList *transport_factory_ids;

void transport_factory_id_global_init(void)
{
}

void transport_factory_id_global_deinit(void)
{
  g_list_free_full(transport_factory_ids, transport_factory_id_free);
  transport_factory_ids = NULL;
}

void
transport_factory_id_register(TransportFactoryId *id)
{
  main_loop_assert_main_thread();
  transport_factory_ids = g_list_append(transport_factory_ids, id);
}

static gpointer
_clone(gconstpointer id)
{
  return g_string_new(((const GString *)id)->str);
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
transport_factory_id_free(gpointer id)
{
  g_string_free((GString *)id, TRUE);
}

const gchar *
transport_factory_id_to_string(const TransportFactoryId *id)
{
  GString *str = (GString *)id;
  return str->str;
}

guint
transport_factory_id_hash(gconstpointer key)
{
  return g_string_hash((const GString *)key);
}

gboolean
transport_factory_id_equal(const TransportFactoryId *id1, const TransportFactoryId *id2)
{
  return g_string_equal(id1, id2);
}

