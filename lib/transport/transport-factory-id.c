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
  gint uniq_id;
};

typedef struct _Sequence Sequence;

struct _Sequence
{
  gint ctr;
} sequence;

static void
sequence_inc(Sequence *seq)
{
  seq->ctr++;
}

static gint
sequence_get(Sequence *seq)
{
  return seq->ctr;
}

static void
sequence_reset(Sequence *seq, gint init_value)
{
  seq->ctr = init_value;
}

static GList *transport_factory_ids;
GStaticMutex transport_factory_ids_mutex;

void
transport_factory_id_global_init(void)
{
  sequence_reset(&sequence, 1);
  g_static_mutex_init(&transport_factory_ids_mutex);
}

static inline void
_free(gpointer s)
{
  transport_factory_id_free((TransportFactoryId *)s);
}

void
transport_factory_id_global_deinit(void)
{
  g_list_free_full(transport_factory_ids, _free);
  transport_factory_ids = NULL;
  g_static_mutex_free(&transport_factory_ids_mutex);
}

void
transport_factory_id_register(TransportFactoryId *id)
{
  transport_factory_ids = g_list_append(transport_factory_ids, id);
}

static gpointer
_clone(gconstpointer s)
{
  TransportFactoryId *id = (TransportFactoryId *)s;
  TransportFactoryId *cloned = g_new0(TransportFactoryId, 1);

  cloned->transport_name = g_strdup(id->transport_name);
  cloned->uniq_id = id->uniq_id;

  return cloned;
}

TransportFactoryId *
transport_factory_id_clone(const TransportFactoryId *id)
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
  g_free(id);
}

guint
transport_factory_id_hash(gconstpointer key)
{
  TransportFactoryId *id = (TransportFactoryId *)key;
  return g_direct_hash(GINT_TO_POINTER(id->uniq_id));
}

gboolean
transport_factory_id_equal(const TransportFactoryId *id1, const TransportFactoryId *id2)
{
  return (id1->uniq_id == id2->uniq_id) ? TRUE : FALSE;
}

const gchar *
transport_factory_id_get_transport_name(const TransportFactoryId *id)
{
  return id->transport_name;
}

void
_transport_factory_id_lock(void)
{
  g_static_mutex_lock(&transport_factory_ids_mutex);
}

void
_transport_factory_id_unlock(void)
{
  g_static_mutex_unlock(&transport_factory_ids_mutex);
}

TransportFactoryId *
transport_factory_id_new(const gchar *transport_name)
{
  TransportFactoryId *id = g_new0(TransportFactoryId, 1);

  sequence_inc(&sequence);
  id->transport_name = g_strdup(transport_name);
  id->uniq_id = sequence_get(&sequence);

  return id;
}
