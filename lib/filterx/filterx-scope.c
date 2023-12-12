#include "filterx/filterx-scope.h"
#include "scratch-buffers.h"

struct _FilterXScope
{
  GHashTable *value_cache;
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
  value->shadow = TRUE;
  g_hash_table_insert(self->value_cache, GINT_TO_POINTER(handle), filterx_object_ref(value));
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
      if (!filterx_object_marshal(value, buffer, &t))
        g_assert_not_reached();
      log_msg_set_value_with_type(msg, handle, buffer->str, buffer->len, t);
    }

}

FilterXScope *
filterx_scope_new(void)
{
  FilterXScope *self = g_new0(FilterXScope, 1);

  self->value_cache = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) filterx_object_unref);
  return self;
}

void
filterx_scope_free(FilterXScope *self)
{
  g_hash_table_unref(self->value_cache);
  g_free(self);
}
