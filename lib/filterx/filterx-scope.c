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

struct _FilterXVariable
{
  NVHandle handle;
  /*
   * floating -- Indicates that this variable is not tied to the log
   *             message, it is a floating variable
   * assigned -- Indicates that the variable was assigned to a new value
   */
  guint32 floating:1,
          assigned:1;
  FilterXObject *value;
};

FilterXObject *
filterx_variable_get_value(FilterXVariable *v)
{
  return filterx_object_ref(v->value);
}

void
filterx_variable_set_value(FilterXVariable *v, FilterXObject *new_value)
{
  filterx_object_unref(v->value);
  v->value = filterx_object_ref(new_value);
  v->assigned = TRUE;
}

static void
_variable_free(FilterXVariable *v)
{
  filterx_object_unref(v->value);
}

struct _FilterXScope
{
  GAtomicCounter ref_cnt;
  GArray *variables;
  GHashTable *by_handle;
  GPtrArray *weak_refs;
  gboolean write_protected;
};

static gboolean
_lookup_variable(FilterXScope *self, NVHandle handle, FilterXVariable **v)
{
  gpointer _value = NULL;

  if (g_hash_table_lookup_extended(self->by_handle, GINT_TO_POINTER(handle), NULL, &_value))
    {
      *v = (FilterXVariable *) _value;
      return *v != NULL;
    }
  return FALSE;
}

FilterXVariable *
filterx_scope_lookup_variable(FilterXScope *self, NVHandle handle)
{
  FilterXVariable *v;

  if (_lookup_variable(self, handle, &v))
    return v;
  return NULL;
}

FilterXVariable *
filterx_scope_register_variable(FilterXScope *self,
                                NVHandle handle, gboolean floating,
                                FilterXObject *initial_value)
{
  FilterXVariable v, *v_slot;

  v.handle = handle;
  v.assigned = FALSE;
  v.floating = floating;
  v.value = filterx_object_ref(initial_value);
  v_slot = &g_array_index(self->variables, FilterXVariable, self->variables->len);
  g_array_append_val(self->variables, v);

  g_hash_table_insert(self->by_handle, GINT_TO_POINTER(handle), v_slot);
  return v_slot;
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

  for (gint i = 0; i < self->variables->len; i++)
    {
      FilterXVariable *v = &g_array_index(self->variables, FilterXVariable, i);

      /* we don't need to sync the value if:
       *
       *  1) this is a floating variable; OR
       *
       *  2) the value was extracted from the message but was not changed in
       *     place (for mutable objects), and was not assigned to
       *
       */
      if (v->floating ||
          !(v->assigned || v->value->modified_in_place))
        continue;
      LogMessageValueType t;
      g_string_truncate(buffer, 0);
      if (!filterx_object_marshal(v->value, buffer, &t))
        g_assert_not_reached();
      log_msg_set_value_with_type(msg, v->handle, buffer->str, buffer->len, t);
      v->value->modified_in_place = FALSE;
      v->assigned = FALSE;
    }
}

FilterXScope *
filterx_scope_new(void)
{
  FilterXScope *self = g_new0(FilterXScope, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->variables = g_array_sized_new(FALSE, TRUE, sizeof(FilterXVariable), 16);
  g_array_set_clear_func(self->variables, (GDestroyNotify) _variable_free);
  self->by_handle = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  self->weak_refs = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  return self;
}

static FilterXScope *
filterx_scope_clone(FilterXScope *other)
{
  FilterXScope *self = filterx_scope_new();

  for (gint i = 0; i < other->variables->len; i++)
    {
      FilterXVariable *v = &g_array_index(other->variables, FilterXVariable, i);
      g_array_append_val(self->variables, *v);
      FilterXVariable *v_clone = &g_array_index(self->variables, FilterXVariable, i);

      v_clone->value = filterx_object_clone(v->value);
      g_hash_table_insert(self->by_handle, GINT_TO_POINTER(v->handle), v_clone);
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
  g_array_free(self->variables, TRUE);
  g_hash_table_unref(self->by_handle);
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
