#include "nvtable.h"
#include "messages.h"

#include <string.h>
#include <stdlib.h>

const gchar *null_string = "";

struct _NVRegistry
{
  /* number of static names that are statically allocated in each payload */
  gint num_static_names;
  GPtrArray *names;
  GHashTable *name_map;
};

NVHandle
nv_registry_get_value_handle(NVRegistry *self, const gchar *name)
{
  gpointer p;
  gchar *stored;
  gsize len;

  p = g_hash_table_lookup(self->name_map, name);
  if (p)
    return GPOINTER_TO_UINT(p);

  len = strlen(name);
  if (len == 0)
    {
      msg_error("Name-value pairs cannot have a zero-length name",
                evt_tag_str("value", name),
                NULL);
      return 0;
    }
  else if (len > 255)
    {
      msg_error("Value names cannot be longer than 255 characters, this value will always expand to the emptry string",
                evt_tag_str("value", name),
                NULL);
      return 0;
    }
  else if (self->names->len >= 65535)
    {
      msg_error("Hard wired limit of 65535 name-value pairs have been reached, all further name-value pair will expand to nothing",
                evt_tag_str("value", name),
                NULL);
      return 0;
    }
  /* first byte is the length, then the zero terminated string */
  stored = g_malloc(len + 3);

  /* memory layout: flags || length || name (NUL terminated) */
  stored[0] = 0;
  stored[1] = len;
  memcpy(&stored[2], name, len+1);
  g_ptr_array_add(self->names, stored);
  g_hash_table_insert(self->name_map, stored + 2, GUINT_TO_POINTER(self->names->len));
  return self->names->len;
}

/**
 * nv_registry_add_alias:
 * @handle: a NV handle to be aliased
 * @alias: must be a caller allocated string pointing to the alias of "handle"
 **/
void
nv_registry_add_alias(NVRegistry *self, NVHandle handle, const gchar *alias)
{
  g_hash_table_insert(self->name_map, (gchar *) alias, GUINT_TO_POINTER(handle));
}

guint8
nv_registry_get_handle_flags(NVRegistry *self, NVHandle handle)
{
  gchar *stored;

  if (G_UNLIKELY(!handle))
    return 0;

  stored = (gchar *) g_ptr_array_index(self->names, handle - 1);
  return stored[0];
}

void
nv_registry_set_handle_flags(NVRegistry *self, NVHandle handle, guint8 flags)
{
  gchar *stored;

  if (G_UNLIKELY(!handle))
    return;

  stored = (gchar *) g_ptr_array_index(self->names, handle - 1);
  stored[0] = flags;
}

const gchar *
nv_registry_get_value_name(NVRegistry *self, NVHandle handle, gssize *length)
{
  gchar *stored;

  if (G_UNLIKELY(!handle))
    {
      if (length)
        *length = 4;
      return "None";
    }

  stored = (gchar *) g_ptr_array_index(self->names, handle - 1);
  if (G_LIKELY(length))
    *length = ((guint8 *) stored)[1];
  return stored + 2;
}

NVRegistry *
nv_registry_new(const gchar **static_names)
{
  NVRegistry *self = g_new0(NVRegistry, 1);
  gint i;

  self->name_map = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
  self->names = g_ptr_array_new();
  for (i = 0; static_names[i]; i++)
    {
      nv_registry_get_value_handle(self, static_names[i]);
    }
  return self;
}

void
nv_registry_free(NVRegistry *self)
{
  g_ptr_array_foreach(self->names, (GFunc) g_free, NULL);
  g_ptr_array_free(self->names, TRUE);
  g_hash_table_destroy(self->name_map);
  g_free(self);
}

/* clonable LogMessage support with shared data pointers */

#define NV_TABLE_DYNVALUE_HANDLE(x)  ((x) >> 16)
#define NV_TABLE_DYNVALUE_OFS(x)  ((x) & 0xFFFF)


static inline gchar *
nv_table_get_bottom(NVTable *self)
{
  return nv_table_get_top(self) - (self->used << NV_TABLE_SCALE);
}

static inline gchar *
nv_table_get_ofs_table_top(NVTable *self)
{
  return (gchar *) &self->data[self->num_static_entries * sizeof(self->static_entries[0]) + self->num_dyn_entries * sizeof(guint32)];
}

static inline NVEntry *
nv_table_get_entry_at_ofs(NVTable *self, guint16 ofs)
{
  if (!ofs)
    return NULL;
  return (NVEntry *) (nv_table_get_top(self) - (ofs << NV_TABLE_SCALE));
}

static inline guint32 *
nv_table_get_dyn_entries(NVTable *self)
{
  return (guint32 *) &self->static_entries[self->num_static_entries];
}

static inline gboolean
nv_table_alloc_check(NVTable *self, gsize alloc_size)
{
  if (nv_table_get_bottom(self) - alloc_size < nv_table_get_ofs_table_top(self))
    return FALSE;
  return TRUE;
}

/* return the offset to a newly allocated payload string */
static inline NVEntry *
nv_table_alloc_value(NVTable *self, gsize alloc_size)
{
  NVEntry *entry;

  alloc_size = NV_TABLE_BOUND(alloc_size);
  /* alloc error, NVTable should be realloced */
  if (!nv_table_alloc_check(self, alloc_size))
    return NULL;
  self->used += alloc_size >> NV_TABLE_SCALE;
  entry = (NVEntry *) (nv_table_get_top(self) - (self->used << NV_TABLE_SCALE));
  entry->alloc_len = alloc_size >> NV_TABLE_SCALE;
  entry->indirect = FALSE;
  entry->referenced = FALSE;
  return entry;
}

static gint
cmp_uints(const void *p1, const void *p2)
{
  if (*(guint32 *) p1 < *(guint32 *) p2)
    {
      return -1;
    }
  else if (*(guint32 *) p1 > *(guint32 *) p2)
    {
      return 1;
    }
  else
    return 0;
}

/* we only support single indirection */
const gchar *
nv_table_resolve_indirect(NVTable *self, NVEntry *entry, gssize *length)
{
  const gchar *referenced_value;
  gssize referenced_length;

  referenced_value = nv_table_get_value(self, entry->vindirect.handle, &referenced_length);
  if (entry->vindirect.ofs > referenced_length)
    return null_string;

  /* here we assume that indirect references are only looked up with
   * non-zero terminated strings properly handled, thus the caller has
   * to supply a non-NULL value_len */

  *length = MIN(entry->vindirect.ofs + entry->vindirect.len, referenced_length) - entry->vindirect.ofs;
  return referenced_value + entry->vindirect.ofs;
}

const inline gchar *
nv_table_resolve_entry(NVTable *self, NVEntry *entry, gssize *length)
{
  if (!entry->indirect)
    {
      if (length)
        *length = entry->vdirect.value_len;
      return entry->vdirect.data + entry->name_len + 1;
    }
  else
    return nv_table_resolve_indirect(self, entry, length);
}

NVEntry *
nv_table_get_entry_slow(NVTable *self, NVHandle handle, guint32 **dyn_slot)
{
  guint16 ofs;
  gint l, h, m;
  guint32 *dyn_entries = nv_table_get_dyn_entries(self);
  guint32 mv;

  if (!self->num_dyn_entries)
    {
      *dyn_slot = NULL;
      return NULL;
    }

  /* search for handle in the dynamic entries section */
  if (!self->dyn_sorted)
    {
      qsort(dyn_entries, self->num_dyn_entries, sizeof(dyn_entries[0]), cmp_uints);
      self->dyn_sorted = 1;
    }

  /* open-coded binary search */
  *dyn_slot = NULL;
  l = 0;
  h = self->num_dyn_entries - 1;
  ofs = 0;
  while (l <= h)
    {
      m = (l+h) >> 1;
      mv = NV_TABLE_DYNVALUE_HANDLE(dyn_entries[m]);
      if (mv == handle)
        {
          *dyn_slot = &dyn_entries[m];
          ofs = NV_TABLE_DYNVALUE_OFS(dyn_entries[m]);
          break;
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

  return nv_table_get_entry_at_ofs(self, ofs);
}

static gboolean
nv_table_reserve_table_entry(NVTable *self, NVHandle handle, guint32 *dyn_slot)
{
  if (!dyn_slot && handle > self->num_static_entries)
    {
      /* this is a dynamic value */
      guint32 *dyn_entries = nv_table_get_dyn_entries(self);;

      if (!nv_table_alloc_check(self, sizeof(dyn_entries[0])))
        return FALSE;

      if (self->num_dyn_entries > 0 && handle < NV_TABLE_DYNVALUE_HANDLE(dyn_entries[self->num_dyn_entries - 1]))
        self->dyn_sorted = 0;
      dyn_entries[self->num_dyn_entries] = (handle << 16) + 0;
      self->num_dyn_entries++;
    }
  return TRUE;
}

static gboolean
nv_table_drop_table_entry(NVTable *self, NVHandle handle, guint32 *dyn_slot)
{
  if (!dyn_slot && handle > self->num_static_entries)
    {
      self->num_dyn_entries--;
    }
  return TRUE;
}

static inline void
nv_table_set_table_entry(NVTable *self, NVHandle handle, guint16 ofs, guint32 *dyn_slot)
{
  if (G_LIKELY(handle <= self->num_static_entries))
    {
      /* this is a statically allocated value, simply store the offset */
      self->static_entries[handle-1] = ofs;
    }
  else
    {
      /* this is a dynamic value */

      if (!dyn_slot)
        {
          guint32 *dyn_entries = nv_table_get_dyn_entries(self);;

          dyn_slot = &dyn_entries[self->num_dyn_entries - 1];
        }
      *dyn_slot = (handle << 16) + ofs;
    }
}

static gboolean
nv_table_make_direct(NVHandle handle, NVEntry *entry, gpointer user_data)
{
  NVTable *self = (NVTable *) (((gpointer *) user_data)[0]);
  NVHandle ref_handle = GPOINTER_TO_UINT(((gpointer *) user_data)[1]);

  if (entry->indirect && entry->vindirect.handle == ref_handle)
    {
      const gchar *value;
      gssize value_len;

      value = nv_table_resolve_indirect(self, entry, &value_len);
      if (!nv_table_add_value(self, handle, entry->vindirect.name, entry->name_len, value, value_len, NULL))
        {
          /* nvtable full, but we can't realloc it ourselves,
           * propagate this back as a failure of
           * nv_table_add_value() */

          return TRUE;
        }
    }
  return FALSE;
}

gboolean
nv_table_add_value(NVTable *self, NVHandle handle, const gchar *name, gsize name_len, const gchar *value, gsize value_len, gboolean *new_entry)
{
  NVEntry *entry;
  guint16 ofs;
  guint32 *dyn_slot;

  if (new_entry)
    *new_entry = FALSE;
  entry = nv_table_get_entry(self, handle, &dyn_slot);
  if (G_UNLIKELY(!entry && !new_entry && value_len == 0))
    {
      /* we don't store zero length matches unless the caller is
       * interested in whether a new entry was created. It is used by
       * the SDATA support code to decide whether a previously
       * not-present SDATA was set */
      return TRUE;
    }
  if (G_UNLIKELY(entry && !entry->indirect && entry->referenced))
    {
      gpointer data[2] = { self, GUINT_TO_POINTER(handle) };

      if (nv_table_foreach_entry(self, nv_table_make_direct, data))
        {
          /* we had to stop iteration, which means that we were unable
           * to allocate enough space for making indirect entries
           * direct */
          return FALSE;
        }
    }
  if (G_UNLIKELY(entry && (((guint) entry->alloc_len << NV_TABLE_SCALE)) >= value_len + NV_ENTRY_DIRECT_HDR + name_len + 2))
    {
      gchar *dst;
      /* this value already exists and the new value fits in the old space */
      if (!entry->indirect)
        {
          dst = entry->vdirect.data + entry->name_len + 1;

          entry->vdirect.value_len = value_len;
          memcpy(dst, value, value_len);
          dst[value_len] = 0;
        }
      else
        {
          /* this was an indirect entry, convert it */
          entry->indirect = 0;
          entry->vdirect.value_len = value_len;
          entry->name_len = name_len;
          memcpy(entry->vdirect.data, name, name_len + 1);
          memcpy(entry->vdirect.data + name_len + 1, value, value_len);
          entry->vdirect.data[entry->name_len + 1 + value_len] = 0;
        }
      return TRUE;
    }
  else if (!entry && new_entry)
    *new_entry = TRUE;

  /* check if there's enough free space: size of the struct plus the
   * size needed for a dynamic table slot */
  if (!nv_table_reserve_table_entry(self, handle, dyn_slot))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_DIRECT_HDR + name_len + value_len + 2);
  if (G_UNLIKELY(!entry))
    {
      nv_table_drop_table_entry(self, handle, dyn_slot);
      return FALSE;
    }

  ofs = (nv_table_get_top(self) - (gchar *) entry) >> NV_TABLE_SCALE;
  entry->vdirect.value_len = value_len;
  entry->name_len = name_len;
  memcpy(entry->vdirect.data, name, name_len + 1);
  memcpy(entry->vdirect.data + name_len + 1, value, value_len);
  entry->vdirect.data[entry->name_len + 1 + value_len] = 0;

  nv_table_set_table_entry(self, handle, ofs, dyn_slot);
  return TRUE;
}

gboolean
nv_table_add_value_indirect(NVTable *self, NVHandle handle, const gchar *name, gsize name_len, NVHandle ref_handle, guint8 type, guint16 rofs, guint16 rlen, gboolean *new_entry)
{
  NVEntry *entry, *ref_entry;
  guint32 *dyn_slot;
  guint16 ofs;

  if (new_entry)
    *new_entry = FALSE;
  ref_entry = nv_table_get_entry(self, ref_handle, &dyn_slot);
  if (ref_entry && ref_entry->indirect)
    {
      const gchar *ref_value;
      gssize ref_length;

      /* NOTE: uh-oh, the to-be-referenced value is already an indirect
       * reference, this is not supported, copy the stuff */

      ref_value = nv_table_resolve_indirect(self, ref_entry, &ref_length);

      if (rofs > ref_length)
        {
          rlen = 0;
          rofs = 0;
        }
      else
        {
          rlen = MIN(rofs + rlen, ref_length) - rofs;
        }
      return nv_table_add_value(self, handle, name, name_len, ref_value + rofs, rlen, new_entry);
    }

  entry = nv_table_get_entry(self, handle, &dyn_slot);
  if (!entry && !new_entry && (rlen == 0 || !ref_entry))
    {
      /* we don't store zero length matches unless the caller is
       * interested in whether a new entry was created. It is used by
       * the SDATA support code to decide whether a previously
       * not-present SDATA was set */

      return TRUE;
    }
  if (entry && !entry->indirect && entry->referenced)
    {
      gpointer data[2] = { self, GUINT_TO_POINTER(handle) };

      if (!nv_table_foreach_entry(self, nv_table_make_direct, data))
        return FALSE;
    }
  if (entry && (((guint) entry->alloc_len << NV_TABLE_SCALE) >= NV_ENTRY_INDIRECT_HDR + name_len + 1))
    {
      /* this value already exists and the new reference  fits in the old space */
      ref_entry->referenced = TRUE;
      entry->vindirect.handle = ref_handle;
      entry->vindirect.ofs = rofs;
      entry->vindirect.len = rlen;
      entry->vindirect.type = type;
      if (!entry->indirect)
        {
          /* previously a non-indirect entry, convert it */
          entry->indirect = 1;
          entry->name_len = name_len;
          memcpy(entry->vindirect.name, name, name_len + 1);
        }
      return TRUE;
    }
  else if (!entry && new_entry)
    *new_entry = TRUE;

  if (!nv_table_reserve_table_entry(self, handle, dyn_slot))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_INDIRECT_HDR + name_len + 1);
  if (!entry)
    {
      nv_table_drop_table_entry(self, handle, dyn_slot);
      return FALSE;
    }

  ofs = (nv_table_get_top(self) - (gchar *) entry) >> NV_TABLE_SCALE;
  entry->vindirect.handle = ref_handle;
  entry->vindirect.ofs = rofs;
  entry->vindirect.len = rlen;
  entry->vindirect.type = type;
  entry->name_len = name_len;
  entry->indirect = 1;
  ref_entry->referenced = TRUE;
  memcpy(entry->vindirect.name, name, name_len + 1);

  nv_table_set_table_entry(self, handle, ofs, dyn_slot);
  return TRUE;
}

static gboolean
nv_table_call_foreach(NVHandle handle, NVEntry *entry, gpointer user_data)
{
  NVTable *self = (NVTable *) ((gpointer *) user_data)[0];
  NVTableForeachFunc func = ((gpointer *) user_data)[1];
  gpointer func_data = ((gpointer *) user_data)[2];
  const gchar *value;
  gssize value_len;

  value = nv_table_resolve_entry(self, entry, &value_len);
  return func(handle, nv_entry_get_name(entry), value, value_len, func_data);
}

gboolean
nv_table_foreach(NVTable *self, NVTableForeachFunc func, gpointer user_data)
{
  gpointer data[3] = { self, func, user_data };

  return nv_table_foreach_entry(self, nv_table_call_foreach, data);
}

gboolean
nv_table_foreach_entry(NVTable *self, NVTableForeachEntryFunc func, gpointer user_data)
{
  guint32 *dyn_entries;
  NVEntry *entry;
  gint i;

  for (i = 0; i < self->num_static_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, self->static_entries[i]);
      if (!entry)
        continue;

      if (func(i + 1, entry, user_data))
        return TRUE;
    }

  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));

      if (!entry)
        continue;

      if (func(NV_TABLE_DYNVALUE_HANDLE(dyn_entries[i]), entry, user_data))
        return TRUE;
    }

  return FALSE;
}


NVTable *
nv_table_new(gint num_static_entries, gint num_dyn_values, gint init_length)
{
  NVTable *self;
  gsize alloc_length;

  alloc_length = NV_TABLE_BOUND(init_length) + NV_TABLE_BOUND(sizeof(NVTable) + num_static_entries * sizeof(self->static_entries[0]) + num_dyn_values * sizeof(guint32));
  self = (NVTable *) g_malloc(alloc_length);

  self->size = alloc_length >> NV_TABLE_SCALE;
  self->used = 0;
  self->num_dyn_entries = 0;
  self->num_static_entries = NV_TABLE_BOUND_NUM_STATIC(num_static_entries);
  self->dyn_sorted = TRUE;
  memset(&self->static_entries[0], 0, self->num_static_entries * sizeof(self->static_entries[0]));
  return self;
}

NVTable *
nv_table_realloc(NVTable *self)
{
  gint old_size = self->size;

  self = g_realloc(self, old_size << (NV_TABLE_SCALE + 1));
  self->size <<= 1;

  memmove(NV_TABLE_ADDR(self, self->size - self->used),
          NV_TABLE_ADDR(self, old_size - self->used),
          self->used << NV_TABLE_SCALE);
  return self;
}

void
nv_table_free(NVTable *self)
{
  g_free(self);
}

/**
 * nv_table_clone:
 * @self: payload to clone
 * @additional_space: specifies how much additional space is needed in
 *                    the newly allocated clone
 *
 **/
NVTable *
nv_table_clone(NVTable *self, gint additional_space)
{
  NVTable *new;
  gint new_size;

  if (nv_table_get_bottom(self) - nv_table_get_ofs_table_top(self) < additional_space)
    new_size = self->size;
  else
    new_size = self->size + (NV_TABLE_BOUND(additional_space) >> NV_TABLE_SCALE);

  new = g_malloc(new_size << NV_TABLE_SCALE);
  memcpy(new, self, sizeof(NVTable) + self->num_static_entries * sizeof(self->static_entries[0]) + self->num_dyn_entries * sizeof(guint32));
  new->size = new_size;

  memcpy(NV_TABLE_ADDR(new, new->size - new->used),
          NV_TABLE_ADDR(self, self->size - self->used),
          self->used << NV_TABLE_SCALE);
  return new;
}
