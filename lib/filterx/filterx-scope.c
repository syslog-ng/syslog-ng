/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "filterx/filterx-scope.h"
#include "scratch-buffers.h"

struct _FilterXScope
{
  GAtomicCounter ref_cnt;
  GHashTable *value_cache;
  GPtrArray *weak_refs;
  gboolean write_protected;
};

FilterXObject *
filterx_scope_lookup_message_ref(FilterXScope *self, NVHandle handle)
{
  FilterXObject *object = NULL;

  if (g_hash_table_lookup_extended(self->value_cache, GINT_TO_POINTER(handle), NULL, (gpointer *) &object))
    {
      filterx_object_ref(object);
    }
  return object;
}

void
filterx_scope_register_message_ref(FilterXScope *self, NVHandle handle, FilterXObject *value)
{
  g_assert(self->write_protected == FALSE);

  value->shadow = TRUE;
  g_hash_table_insert(self->value_cache, GINT_TO_POINTER(handle), filterx_object_ref(value));
}

void
filterx_scope_store_weak_ref(FilterXScope *self, FilterXObject *object)
{
  g_assert(self->write_protected == FALSE);

  if (object)
    g_ptr_array_add(self->weak_refs, filterx_object_ref(object));
}

void
filterx_scope_sync_to_message(FilterXScope *self, LogMessage *msg)
{
  GString *buffer = scratch_buffers_alloc();
  GHashTableIter iter;
  gpointer _key, _value;

  g_hash_table_iter_init(&iter, self->value_cache);
  while (g_hash_table_iter_next(&iter, &_key, &_value))
    {
      NVHandle handle = GPOINTER_TO_INT(_key);
      FilterXObject *value = (FilterXObject *) _value;

      if (!(value->modified_in_place || value->assigned))
        continue;
      LogMessageValueType t;
      g_string_truncate(buffer, 0);
      if (!filterx_object_marshal(value, buffer, &t))
        g_assert_not_reached();
      log_msg_set_value_with_type(msg, handle, buffer->str, buffer->len, t);
    }

}

FilterXScope *
filterx_scope_new(void)
{
  FilterXScope *self = g_new0(FilterXScope, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->value_cache = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) filterx_object_unref);
  self->weak_refs = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  return self;
}

static FilterXScope *
filterx_scope_clone(FilterXScope *other)
{
  FilterXScope *self = filterx_scope_new();

  GHashTableIter iter;
  gpointer _key, _value;

  g_hash_table_iter_init(&iter, self->value_cache);
  while (g_hash_table_iter_next(&iter, &_key, &_value))
    {
      NVHandle handle = GPOINTER_TO_INT(_key);
      FilterXObject *value = (FilterXObject *) _value;

      /* NOTE: clone will not actually clone inmutable objects, in those
       * cases we just take a reference */
      g_hash_table_insert(self->value_cache, GINT_TO_POINTER(handle), filterx_object_clone(value));
    }

  /* NOTE: we don't clone weak references, those only relate to mutable
   * objects, which we are cloning anyway */
  return self;
}

void
filterx_scope_write_protect(FilterXScope *self)
{
  self->write_protected = TRUE;
}

FilterXScope *
filterx_scope_make_writable(FilterXScope **pself)
{
  if ((*pself)->write_protected)
    {
      FilterXScope *new;

      new = filterx_scope_clone(*pself);
      filterx_scope_unref(*pself);
      *pself = new;
    }
  return *pself;
}

static void
_free(FilterXScope *self)
{
  g_hash_table_unref(self->value_cache);
  g_ptr_array_free(self->weak_refs, TRUE);
  g_free(self);
}

FilterXScope *
filterx_scope_ref(FilterXScope *self)
{
  if (self)
    g_atomic_counter_inc(&self->ref_cnt);
  return self;
}

void
filterx_scope_unref(FilterXScope *self)
{
  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _free(self);
}
