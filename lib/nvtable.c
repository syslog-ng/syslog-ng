/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include "nvtable.h"
#include "messages.h"

#include <string.h>
#include <stdlib.h>

GStaticMutex nv_registry_lock = G_STATIC_MUTEX_INIT;

const gchar *null_string = "";

NVHandle
nv_registry_get_handle(NVRegistry *self, const gchar *name)
{
  gpointer p;

  p = g_hash_table_lookup(self->name_map, name);
  if (p)
    return GPOINTER_TO_UINT(p);
  return 0;
}

NVHandle
nv_registry_alloc_handle(NVRegistry *self, const gchar *name)
{
  gpointer p;
  NVHandleDesc stored;
  gsize len;
  NVHandle res = 0;

  g_static_mutex_lock(&nv_registry_lock);
  p = g_hash_table_lookup(self->name_map, name);
  if (p)
    {
      res = GPOINTER_TO_UINT(p);
      goto exit;
    }

  len = strlen(name);
  if (len == 0)
    {
      msg_error("Name-value pairs cannot have a zero-length name",
                evt_tag_str("value", name),
                NULL);
      goto exit;
    }
  else if (len > 255)
    {
      msg_error("Value names cannot be longer than 255 characters, this value will always expand to the emptry string",
                evt_tag_str("value", name),
                NULL);
      goto exit;
    }
  else if (self->names->len >= 65535)
    {
      msg_error("Hard wired limit of 65535 name-value pairs have been reached, all further name-value pair will expand to nothing",
                evt_tag_str("value", name),
                NULL);
      goto exit;
    }
  /* flags (2 bytes) || length (1 byte) || name (len bytes) || NUL */
  /* memory layout: flags || length || name (NUL terminated) */
  stored.flags = 0;
  stored.name_len = len;
  stored.name = g_strdup(name);
  g_array_append_val(self->names, stored);
  g_hash_table_insert(self->name_map, stored.name, GUINT_TO_POINTER(self->names->len));
  res = self->names->len;
 exit:
  g_static_mutex_unlock(&nv_registry_lock);
  return res;
}

/**
 * nv_registry_add_alias:
 * @handle: a NV handle to be aliased
 * @alias: must be a caller allocated string pointing to the alias of "handle"
 **/
void
nv_registry_add_alias(NVRegistry *self, NVHandle handle, const gchar *alias)
{
  g_static_mutex_lock(&nv_registry_lock);
  g_hash_table_insert(self->name_map, (gchar *) alias, GUINT_TO_POINTER((glong) handle));
  g_static_mutex_unlock(&nv_registry_lock);
}

void
nv_registry_set_handle_flags(NVRegistry *self, NVHandle handle, guint16 flags)
{
  NVHandleDesc *stored;

  if (G_UNLIKELY(!handle))
    return;

  stored = &g_array_index(self->names, NVHandleDesc, handle - 1);
  stored->flags = flags;
}

NVRegistry *
nv_registry_new(const gchar **static_names)
{
  NVRegistry *self = g_new0(NVRegistry, 1);
  gint i;

  self->name_map = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
  self->names = g_array_new(FALSE, FALSE, sizeof(NVHandleDesc));
  for (i = 0; static_names[i]; i++)
    {
      nv_registry_alloc_handle(self, static_names[i]);
    }
  return self;
}

void
nv_registry_free(NVRegistry *self)
{
  gint i;

  for (i = 0; i < self->names->len; i++)
    g_free(g_array_index(self->names, NVHandleDesc, i).name);
  g_array_free(self->names, TRUE);
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

static const inline gchar *
nv_table_resolve_entry(NVTable *self, NVEntry *entry, gssize *length)
{
  if (!entry->indirect)
    {
      if (length)
        *length = entry->vdirect.value_len_lo + (entry->vdirect.value_len_hi << 16);
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
nv_table_reserve_table_entry(NVTable *self, NVHandle handle, guint32 **dyn_slot)
{
  if (G_UNLIKELY(!(*dyn_slot) && handle > self->num_static_entries))
    {
      /* this is a dynamic value */
      guint32 *dyn_entries = nv_table_get_dyn_entries(self);;
      gint l, h, m, ndx;
      gboolean found = FALSE;

      if (!nv_table_alloc_check(self, sizeof(dyn_entries[0])))
        return FALSE;

      l = 0;
      h = self->num_dyn_entries - 1;
      ndx = -1;
      while (l <= h)
        {
          guint16 mv;

          m = (l+h) >> 1;
          mv = NV_TABLE_DYNVALUE_HANDLE(dyn_entries[m]);

          if (mv == handle)
            {
              ndx = m;
              found = TRUE;
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
      /* if we find the proper slot we set that, if we don't, we insert a new entry */
      if (!found)
        ndx = l;

      g_assert(ndx >= 0 && ndx <= self->num_dyn_entries);
      if (ndx < self->num_dyn_entries)
        {
          memmove(&dyn_entries[ndx + 1], &dyn_entries[ndx], (self->num_dyn_entries - ndx) * sizeof(dyn_entries[0]));
        }

      *dyn_slot = &dyn_entries[ndx];

      /* we set ofs to zero here, which means that the NVEntry won't
         be found even if the slot is present in dyn_entries */
      **dyn_slot = (handle << 16) + 0;
      if (!found)
        self->num_dyn_entries++;
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

  if (value_len > 255 * 1024)
    value_len = 255 * 1024;
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
      gpointer data[2] = { self, GUINT_TO_POINTER((glong) handle) };

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

          entry->vdirect.value_len_lo = value_len & 0xFFFF;
          entry->vdirect.value_len_hi = (value_len >> 16);
          memcpy(dst, value, value_len);
          dst[value_len] = 0;
        }
      else
        {
          /* this was an indirect entry, convert it */
          entry->indirect = 0;
          entry->vdirect.value_len_lo = value_len & 0xFFFF;
          entry->vdirect.value_len_hi = (value_len >> 16);
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
  if (!nv_table_reserve_table_entry(self, handle, &dyn_slot))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_DIRECT_HDR + name_len + value_len + 2);
  if (G_UNLIKELY(!entry))
    {
      return FALSE;
    }

  ofs = (nv_table_get_top(self) - (gchar *) entry) >> NV_TABLE_SCALE;
  entry->vdirect.value_len_lo = value_len & 0xFFFF;
  entry->vdirect.value_len_hi = value_len >> 16;
  if (handle >= self->num_static_entries)
    {
      /* we only store the name for non-builtin values */
      entry->name_len = name_len;
      memcpy(entry->vdirect.data, name, name_len + 1);
    }
  else
    entry->name_len = 0;
  memcpy(entry->vdirect.data + entry->name_len + 1, value, value_len);
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
      gpointer data[2] = { self, GUINT_TO_POINTER((glong) handle) };

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
          if (handle >= self->num_static_entries)
            {
              entry->name_len = name_len;
              memcpy(entry->vindirect.name, name, name_len + 1);
            }
          else
            {
              entry->name_len = 0;
            }
        }
      return TRUE;
    }
  else if (!entry && new_entry)
    *new_entry = TRUE;

  if (!nv_table_reserve_table_entry(self, handle, &dyn_slot))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_INDIRECT_HDR + name_len + 1);
  if (!entry)
    {
      return FALSE;
    }

  ofs = (nv_table_get_top(self) - (gchar *) entry) >> NV_TABLE_SCALE;
  entry->vindirect.handle = ref_handle;
  entry->vindirect.ofs = rofs;
  entry->vindirect.len = rlen;
  entry->vindirect.type = type;
  entry->indirect = 1;
  ref_entry->referenced = TRUE;
  if (handle >= self->num_static_entries)
    {
      entry->name_len = name_len;
      memcpy(entry->vindirect.name, name, name_len + 1);
    }
  else
    entry->name_len = 0;

  nv_table_set_table_entry(self, handle, ofs, dyn_slot);
  return TRUE;
}

static gboolean
nv_table_call_foreach(NVHandle handle, NVEntry *entry, gpointer user_data)
{
  NVTable *self = (NVTable *) ((gpointer *) user_data)[0];
  NVRegistry *registry = (NVRegistry *) ((gpointer *) user_data)[1];
  NVTableForeachFunc func = ((gpointer *) user_data)[2];
  gpointer func_data = ((gpointer *) user_data)[3];
  const gchar *value;
  gssize value_len;

  value = nv_table_resolve_entry(self, entry, &value_len);
  return func(handle, nv_registry_get_handle_name(registry, handle, NULL), value, value_len, func_data);
}

gboolean
nv_table_foreach(NVTable *self, NVRegistry *registry, NVTableForeachFunc func, gpointer user_data)
{
  gpointer data[4] = { self, registry, func, user_data };

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

void
nv_table_clear(NVTable *self)
{
  g_assert(self->ref_cnt == 1);
  self->used = 0;
  self->num_dyn_entries = 0;
  memset(&self->static_entries[0], 0, self->num_static_entries * sizeof(self->static_entries[0]));
}

void
nv_table_init(NVTable *self, gsize alloc_length, gint num_static_entries)
{
  self->size = alloc_length >> NV_TABLE_SCALE;
  self->used = 0;
  self->num_dyn_entries = 0;
  self->num_static_entries = NV_TABLE_BOUND_NUM_STATIC(num_static_entries);
  self->ref_cnt = 1;
  self->borrowed = FALSE;
  memset(&self->static_entries[0], 0, self->num_static_entries * sizeof(self->static_entries[0]));
}

NVTable *
nv_table_new(gint num_static_entries, gint num_dyn_values, gint init_length)
{
  NVTable *self;
  gsize alloc_length;

  alloc_length = nv_table_get_alloc_size(num_static_entries, num_dyn_values, init_length);
  self = (NVTable *) g_malloc(alloc_length);

  nv_table_init(self, alloc_length, num_static_entries);
  return self;
}

NVTable *
nv_table_init_borrowed(gpointer space, gsize space_len, gint num_static_entries)
{
  NVTable *self = (NVTable *) space;

  space_len &= ~3;
  g_assert(space_len > num_static_entries * sizeof(self->static_entries[0]) + sizeof(NVTable));
  nv_table_init(self, NV_TABLE_BOUND(space_len), num_static_entries);
  self->borrowed = TRUE;
  return self;
}

NVTable *
nv_table_realloc(NVTable *self)
{
  gint old_size = self->size;
  NVTable *new = NULL;

  if (self->ref_cnt == 1 && !self->borrowed)
    {
      self = g_realloc(self, old_size << (NV_TABLE_SCALE + 1));

      self->size <<= 1;
      memmove(NV_TABLE_ADDR(self, self->size - self->used),
              NV_TABLE_ADDR(self, old_size - self->used),
              self->used << NV_TABLE_SCALE);
    }
  else
    {
      new = g_malloc(old_size << (NV_TABLE_SCALE + 1));

      /* we only copy the header in this case */
      memcpy(new, self, sizeof(NVTable) + self->num_static_entries * sizeof(self->static_entries[0]) + self->num_dyn_entries * sizeof(guint32));
      self->size <<= 1;
      new->ref_cnt = 1;
      new->borrowed = FALSE;

      memmove(NV_TABLE_ADDR(new, new->size - new->used),
              NV_TABLE_ADDR(self, old_size - self->used),
              self->used << NV_TABLE_SCALE);

      nv_table_unref(self);
      self = new;
    }
  return self;
}

NVTable *
nv_table_ref(NVTable *self)
{
  self->ref_cnt++;
  return self;
}

void
nv_table_unref(NVTable *self)
{
  if ((--self->ref_cnt == 0) && !self->borrowed)
    {
      g_free(self);
    }
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
  new->ref_cnt = 1;
  new->borrowed = FALSE;

  memcpy(NV_TABLE_ADDR(new, new->size - new->used),
          NV_TABLE_ADDR(self, self->size - self->used),
          self->used << NV_TABLE_SCALE);
  return new;
}
