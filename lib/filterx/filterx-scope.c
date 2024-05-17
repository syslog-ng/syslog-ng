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

#define FILTERX_HANDLE_FLOATING_BIT (1UL << 31)
#define FILTERX_SCOPE_MAX_GENERATION ((1UL << 20) - 1)

struct _FilterXVariable
{
  /* the MSB indicates that the variable is a floating one */
  FilterXVariableHandle handle;
  /*
   * assigned -- Indicates that the variable was assigned to a new value
   *
   * declared -- this variable is declared (e.g. retained for the entire input pipeline)
   */
  guint32 assigned:1,
          declared:1,
          generation:20;
  FilterXObject *value;
};

gboolean
filterx_variable_handle_is_floating(FilterXVariableHandle handle)
{
  return !!(handle & FILTERX_HANDLE_FLOATING_BIT);
}

gboolean
filterx_variable_is_floating(FilterXVariable *v)
{
  return filterx_variable_handle_is_floating(v->handle);
}

static NVHandle
filterx_variable_get_nv_handle(FilterXVariable *v)
{
  return v->handle & ~FILTERX_HANDLE_FLOATING_BIT;
}

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

void
filterx_variable_unset_value(FilterXVariable *v)
{
  filterx_variable_set_value(v, NULL);
}

gboolean
filterx_variable_is_set(FilterXVariable *v)
{
  return v->value != NULL;
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
  guint32 generation:20, write_protected, dirty, syncable;
};

static gboolean
_lookup_variable(FilterXScope *self, FilterXVariableHandle handle, FilterXVariable **v_slot)
{
  gint l, h, m;

  /* open-coded binary search */
  l = 0;
  h = self->variables->len - 1;
  while (l <= h)
    {
      m = (l + h) >> 1;

      FilterXVariable *m_elem = &g_array_index(self->variables, FilterXVariable, m);

      FilterXVariableHandle mv = m_elem->handle;
      if (mv == handle)
        {
          *v_slot = m_elem;
          return TRUE;
        }
      else if (mv > handle)
        {
          h = m - 1;
        }
      else
        {
          l = m + 1;
        }
    }
  *v_slot = &g_array_index(self->variables, FilterXVariable, l);
  return FALSE;
}

void
filterx_scope_set_dirty(FilterXScope *self)
{
  self->dirty = TRUE;
}

gboolean
filterx_scope_is_dirty(FilterXScope *self)
{
  return self->dirty;
}

FilterXVariableHandle
filterx_scope_map_variable_to_handle(const gchar *name, FilterXVariableType type)
{
  NVHandle nv_handle = log_msg_get_value_handle(name);

  if (type == FX_VAR_MESSAGE)
    return (FilterXVariableHandle) nv_handle;
  return (FilterXVariableHandle) nv_handle | FILTERX_HANDLE_FLOATING_BIT;
}

FilterXVariable *
filterx_scope_lookup_variable(FilterXScope *self, FilterXVariableHandle handle)
{
  FilterXVariable *v;

  if (_lookup_variable(self, handle, &v))
    {
      if (filterx_variable_handle_is_floating(handle) &&
          !v->declared && v->generation != self->generation)
        return NULL;
      return v;
    }
  return NULL;
}

static FilterXVariable *
_register_variable(FilterXScope *self,
                   FilterXVariableHandle handle,
                   FilterXObject *initial_value)
{
  FilterXVariable v, *v_slot;

  if (_lookup_variable(self, handle, &v_slot))
    {
      /* already present */
      if (v_slot->generation != self->generation)
        {
          /* existing value is from a previous generation, override it as if
           * it was a new value */

          v_slot->generation = self->generation;
          filterx_variable_set_value(v_slot, initial_value);
          /* consider this to be unset just as an initial registration is */
          v_slot->assigned = FALSE;
        }
      return v_slot;
    }
  /* turn v_slot into an index */
  gsize v_index = ((guint8 *) v_slot - (guint8 *) self->variables->data) / sizeof(FilterXVariable);
  g_assert(v_index <= self->variables->len);
  g_assert(&g_array_index(self->variables, FilterXVariable, v_index) == v_slot);

  v.handle = handle;
  v.assigned = FALSE;
  v.value = filterx_object_ref(initial_value);
  v.generation = self->generation;
  g_array_insert_val(self->variables, v_index, v);

  return &g_array_index(self->variables, FilterXVariable, v_index);
}

FilterXVariable *
filterx_scope_register_variable(FilterXScope *self,
                                FilterXVariableHandle handle,
                                FilterXObject *initial_value)
{
  FilterXVariable *v = _register_variable(self, handle, initial_value);
  v->declared = FALSE;

  /* the scope needs to be synced with the message if it holds a
   * message-tied variable (e.g.  $MSG) */
  if (!filterx_variable_handle_is_floating(handle))
    self->syncable = TRUE;
  return v;
}

FilterXVariable *
filterx_scope_register_declared_variable(FilterXScope *self,
                                         FilterXVariableHandle handle,
                                         FilterXObject *initial_value)

{
  g_assert(filterx_variable_handle_is_floating(handle));

  FilterXVariable *v = _register_variable(self, handle, initial_value);
  v->declared = TRUE;
  return v;
}

/*
 * 1) sync objects to message
 * 2) drop undeclared objects
 */
void
filterx_scope_sync(FilterXScope *self, LogMessage *msg)
{

  if (!self->dirty)
    {
      msg_trace("Filterx sync: not syncing as scope is not dirty",
                evt_tag_printf("scope", "%p", self));
      return;
    }
  if (!self->syncable)
    {
      msg_trace("Filterx sync: not syncing as there are no variables to sync",
                evt_tag_printf("scope", "%p", self));
      self->dirty = FALSE;
      return;
    }

  GString *buffer = scratch_buffers_alloc();

  for (gint i = 0; i < self->variables->len; i++)
    {
      FilterXVariable *v = &g_array_index(self->variables, FilterXVariable, i);

      /* we don't need to sync the value if:
       *
       *  1) this is a floating variable; OR
       *
       *  2) the value was extracted from the message but was not changed in
       *     place (for mutable objects), and was not assigned to.
       *
       */
      if (filterx_variable_is_floating(v))
        {
          /* we could unset undeclared, floating values here as we are
           * transitioning to the next filterx block.  But this is also
           * addressed by the variable generation counter mechanism.  This
           * means that we will unset those if some expr actually refers to
           * it.  Also, we skip the entire sync exercise, unless we have
           * message tied values, so we need to work even if floating
           * variables are not synced.
           *
           * With that said, let's not clear these.
           */
        }
      else if (v->value == NULL)
        {
          msg_trace("Filterx sync: whiteout variable, unsetting in message",
                    evt_tag_str("variable", log_msg_get_value_name(filterx_variable_get_nv_handle(v), NULL)));
          /* we need to unset */
          log_msg_unset_value(msg, v->handle);
          v->assigned = FALSE;
        }
      else if (v->assigned || v->value->modified_in_place)
        {
          LogMessageValueType t;

          msg_trace("Filterx sync: changed variable in scope, overwriting in message",
                    evt_tag_str("variable", log_msg_get_value_name(filterx_variable_get_nv_handle(v), NULL)));

          g_string_truncate(buffer, 0);
          if (!filterx_object_marshal(v->value, buffer, &t))
            g_assert_not_reached();
          log_msg_set_value_with_type(msg, v->handle, buffer->str, buffer->len, t);
          v->value->modified_in_place = FALSE;
          v->assigned = FALSE;
        }
      else
        {
          msg_trace("Filterx sync: variable in scope and message in sync, not doing anything",
                    evt_tag_str("variable", log_msg_get_value_name(filterx_variable_get_nv_handle(v), NULL)));
        }
    }
  self->dirty = FALSE;
}

FilterXScope *
filterx_scope_new(void)
{
  FilterXScope *self = g_new0(FilterXScope, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->variables = g_array_sized_new(FALSE, TRUE, sizeof(FilterXVariable), 16);
  g_array_set_clear_func(self->variables, (GDestroyNotify) _variable_free);
  return self;
}

static FilterXScope *
filterx_scope_clone(FilterXScope *other)
{
  FilterXScope *self = filterx_scope_new();

  for (gint src_index = 0, dst_index = 0; src_index < other->variables->len; src_index++)
    {
      FilterXVariable *v = &g_array_index(other->variables, FilterXVariable, src_index);

      if (v->declared || !filterx_variable_is_floating(v))
        {
          g_array_append_val(self->variables, *v);
          FilterXVariable *v_clone = &g_array_index(self->variables, FilterXVariable, dst_index);

          v_clone->generation = 0;
          if (v->value)
            v_clone->value = filterx_object_clone(v->value);
          else
            v_clone->value = NULL;
          dst_index++;
          msg_trace("Filterx scope, cloning scope variable",
                    evt_tag_str("variable", log_msg_get_value_name((v->handle & ~FILTERX_HANDLE_FLOATING_BIT), NULL)));
        }
    }

  if (other->variables->len > 0)
    self->dirty = other->dirty;
  self->syncable = other->syncable;
  msg_trace("Filterx clone finished",
            evt_tag_printf("scope", "%p", self),
            evt_tag_printf("other", "%p", other),
            evt_tag_int("dirty", self->dirty),
            evt_tag_int("syncable", self->syncable),
            evt_tag_int("write_protected", self->write_protected));
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
  (*pself)->generation++;
  g_assert((*pself)->generation < FILTERX_SCOPE_MAX_GENERATION);
  return *pself;
}

static void
_free(FilterXScope *self)
{
  g_array_free(self->variables, TRUE);
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
