/*
 * Copyright (c) 2002-2019 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@oneidentity.com>
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

#include "signal-slot-connector.h"

typedef struct _SlotFunctor SlotFunctor;

struct _SlotFunctor
{
  Slot slot;
  gpointer object;
};

static SlotFunctor *
_slot_functor_new(Slot slot, gpointer object)
{
  SlotFunctor *self = g_new0(SlotFunctor, 1);

  self->slot = slot;
  self->object = object;

  return self;
}

static void
_slot_functor_free(gpointer data)
{
  SlotFunctor *self = (SlotFunctor *) data;
  g_free(self);
}

static gboolean
_slot_functor_eq(const SlotFunctor *a, const SlotFunctor *b)
{
  return ((a->slot == b->slot) && (a->object == b->object));
}

static gint
_slot_functor_cmp(gconstpointer a, gconstpointer b)
{
  const SlotFunctor *slot_obj_a = (const SlotFunctor *)a;
  const SlotFunctor *slot_obj_b = (const SlotFunctor *)b;

  if (_slot_functor_eq(slot_obj_a, slot_obj_b))
    return 0;

  return -1;
}

struct _SignalSlotConnector
{
  // map<Signal, set<SlotFunctor>> connections;
  GHashTable *connections;
  GMutex *lock; // connect/disconnect guarded by lock, emit is not
};

static GList *
_slot_lookup_node(GList *slot_functors, Slot slot, gpointer object)
{
  for (GList *it = slot_functors; it != NULL; it = it->next)
    {
      SlotFunctor *s = (SlotFunctor *) it->data;

      if (s->slot == slot && s->object == object)
        return it;
    }

  return NULL;
}

static SlotFunctor *
_slot_lookup(GList *slot_functors, Slot slot, gpointer object)
{
  GList *slot_node = _slot_lookup_node(slot_functors, slot, object);
  if (!slot_node)
    return NULL;

  return (SlotFunctor *) slot_node->data;
}

void
signal_slot_connect(SignalSlotConnector *self, Signal signal, Slot slot, gpointer object)
{
  g_assert(signal != NULL);
  g_assert(slot != NULL);

  g_mutex_lock(self->lock);

  GList *slots = g_hash_table_lookup(self->connections, signal);

  gboolean signal_registered = (slots != NULL);

  if (_slot_lookup(slots, slot, object))
    {
      msg_debug("SignalSlotConnector::connect",
                evt_tag_printf("already_connected",
                               "connect(connector=%p,signal=%s,slot=%p, object=%p)",
                               self, signal, slot, object));
      goto exit_;
    }

  GList *new_slots = g_list_append(slots, _slot_functor_new(slot, object));

  if (!signal_registered)
    {
      g_hash_table_insert(self->connections, (gpointer)signal, new_slots);
    }

  msg_debug("SignalSlotConnector::connect",
            evt_tag_printf("new connection registered",
                           "connect(connector=%p,signal=%s,slot=%p,object=%p)",
                           self, signal, slot, object));
exit_:
  g_mutex_unlock(self->lock);
}

static void
_hash_table_replace(GHashTable *hash_table, gpointer key, gpointer new_value)
{
  g_hash_table_steal(hash_table, key);
  gboolean inserted_as_new = g_hash_table_insert(hash_table, key, new_value);
  g_assert(inserted_as_new);
}

void
signal_slot_disconnect(SignalSlotConnector *self, Signal signal, Slot slot, gpointer object)
{
  g_assert(signal != NULL);
  g_assert(slot != NULL);

  g_mutex_lock(self->lock);

  GList *slots = g_hash_table_lookup(self->connections, signal);

  if (!slots)
    goto exit_;

  msg_debug("SignalSlotConnector::disconnect",
            evt_tag_printf("connector", "%p", self),
            evt_tag_str("signal", signal),
            evt_tag_printf("slot", "%p", slot),
            evt_tag_printf("object", "%p", object));

  SlotFunctor slotfunctor =
  {
    .slot = slot,
    .object = object
  };

  GList *slotfunctor_node = g_list_find_custom(slots, &slotfunctor, _slot_functor_cmp);
  if (!slotfunctor_node)
    {
      msg_debug("SignalSlotConnector::disconnect slot object not found",
                evt_tag_printf("connector", "%p", self),
                evt_tag_str("signal", signal),
                evt_tag_printf("slot", "%p", slot),
                evt_tag_printf("object", "%p", object));
      goto exit_;
    }

  GList *new_slots = g_list_remove_link(slots, slotfunctor_node);

  if (!new_slots)
    {
      g_hash_table_remove(self->connections, signal);
      msg_debug("SignalSlotConnector::disconnect last slot is disconnected, unregister signal",
                evt_tag_printf("connector", "%p", self),
                evt_tag_str("signal", signal),
                evt_tag_printf("slot", "%p", slot),
                evt_tag_printf("object", "%p", object));
      goto exit_;
    }

  if (new_slots != slots)
    {
      _hash_table_replace(self->connections, (gpointer)signal, new_slots);
    }

  g_list_free_full(slotfunctor_node, _slot_functor_free);

exit_:
  g_mutex_unlock(self->lock);
}

static void
_run_slot(gpointer data, gpointer user_data)
{
  g_assert(data);

  SlotFunctor *slotfunctor = (SlotFunctor *)data;
  slotfunctor->slot(slotfunctor->object, user_data);
}

void
signal_slot_emit(SignalSlotConnector *self, Signal signal, gpointer user_data)
{
  g_assert(signal != NULL);

  msg_debug("SignalSlotConnector::emit",
            evt_tag_printf("connector", "%p", self),
            evt_tag_str("signal", signal),
            evt_tag_printf("user_data", "%p", user_data));

  GList *slots = g_hash_table_lookup(self->connections, signal);

  if (!slots)
    {
      msg_debug("SignalSlotConnector: unregistered signal emitted",
                evt_tag_printf("connector", "%p", self),
                evt_tag_str("signal", signal));
      return;
    }

  g_list_foreach(slots, _run_slot, user_data);
}

static void
_destroy_list_of_slots(gpointer data)
{
  if (!data)
    return;

  GList *list = (GList *)data;
  g_list_free_full(list, _slot_functor_free);
}

SignalSlotConnector *
signal_slot_connector_new(void)
{
  SignalSlotConnector *self = g_new0(SignalSlotConnector, 1);

  self->connections = g_hash_table_new_full(g_str_hash,
                                            g_str_equal,
                                            NULL,
                                            _destroy_list_of_slots);

  self->lock = g_mutex_new();

  return self;
}

void
signal_slot_connector_free(SignalSlotConnector *self)
{
  g_mutex_free(self->lock);
  g_hash_table_unref(self->connections);
  g_free(self);
}

