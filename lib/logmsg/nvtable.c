/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "logmsg/nvtable.h"
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
    goto exit;

  if (len > 255)
    {
      msg_error("Value names cannot be longer than 255 characters, "
                "this value will always expand to the emptry string",
                evt_tag_str("value", name));
      goto exit;
    }

  if (self->names->len >= self->nvhandle_max_value)
    {
      msg_error("Hard wired limit of name-value pairs have been reached, "
                "all further name-value pair will expand to nothing",
                evt_tag_printf("limit", "%"G_GUINT32_FORMAT, self->nvhandle_max_value),
                evt_tag_str("value", name));
      goto exit;
    }

  /* name (len bytes) || NULL || flags (2 bytes) || length (1 byte)  */
  /* memory layout: name (NUL terminated) || flags || length */
  stored.flags = 0;
  stored.name_len = len;
  stored.name = g_strdup(name);
  nvhandle_desc_array_append(self->names, &stored);
  g_hash_table_insert(self->name_map, g_strdup(name), GUINT_TO_POINTER(self->names->len));
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
  g_hash_table_insert(self->name_map, g_strdup(alias), GUINT_TO_POINTER((glong) handle));
  g_static_mutex_unlock(&nv_registry_lock);
}

void
nv_registry_set_handle_flags(NVRegistry *self, NVHandle handle, guint16 flags)
{
  NVHandleDesc *stored;

  if (G_UNLIKELY(!handle))
    return;

  stored = &nvhandle_desc_array_index(self->names, handle - 1);
  stored->flags = flags;
}

void
nv_registry_foreach(NVRegistry *self, GHFunc callback, gpointer user_data)
{
  g_hash_table_foreach(self->name_map, callback, user_data);
}

NVRegistry *
nv_registry_new(const gchar **static_names, guint32 nvhandle_max_value)
{
  NVRegistry *self = g_new0(NVRegistry, 1);
  gint i;

  self->nvhandle_max_value = nvhandle_max_value;
  self->name_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->names = nvhandle_desc_array_new(NVHANDLE_DESC_ARRAY_INITIAL_SIZE);
  for (i = 0; static_names[i]; i++)
    {
      nv_registry_alloc_handle(self, static_names[i]);
    }
  return self;
}

void
nv_registry_free(NVRegistry *self)
{
  nvhandle_desc_array_free(self->names);
  g_hash_table_destroy(self->name_map);
  g_free(self);
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
  self->used += alloc_size;
  entry = (NVEntry *) (nv_table_get_top(self) - (self->used));

  /* initialize all flags to zero, so we don't need to bump the version for
   * a compatible change */
  entry->flags = 0;
  entry->alloc_len = alloc_size;
  entry->indirect = FALSE;
  entry->referenced = FALSE;
  entry->unset = FALSE;
  return entry;
}

/* we only support single indirection */
const gchar *
nv_table_resolve_indirect(NVTable *self, NVEntry *entry, gssize *length)
{
  const gchar *referenced_value;
  gssize referenced_length;

  g_assert(entry->indirect);

  referenced_value = nv_table_get_value(self, entry->vindirect.handle, &referenced_length);
  if (entry->vindirect.ofs > referenced_length)
    {
      if (length)
        *length = 0;
      return null_string;
    }

  /* here we assume that indirect references are only looked up with
   * non-zero terminated strings properly handled, thus the caller has
   * to supply a non-NULL value_len */

  g_assert(length != NULL);

  *length = MIN(entry->vindirect.ofs + entry->vindirect.len, referenced_length) - entry->vindirect.ofs;
  return referenced_value + entry->vindirect.ofs;
}

static inline const gchar *
nv_table_resolve_direct(NVTable *self, NVEntry *entry, gssize *length)
{
  g_assert(!entry->indirect);

  if (length)
    *length = entry->vdirect.value_len;
  return entry->vdirect.data + entry->name_len + 1;
}

static inline const gchar *
nv_table_resolve_entry(NVTable *self, NVEntry *entry, gssize *length)
{
  if (entry->unset)
    {
      if (length)
        *length = 0;
      return null_string;
    }
  else if (entry->indirect)
    return nv_table_resolve_indirect(self, entry, length);
  else
    return nv_table_resolve_direct(self, entry, length);
}

NVEntry *
nv_table_get_entry_slow(NVTable *self, NVHandle handle, NVIndexEntry **index_entry)
{
  guint32 ofs;
  gint l, h, m;
  NVIndexEntry *index_table = nv_table_get_index(self);
  guint32 mv;

  if (!self->index_size)
    {
      *index_entry = NULL;
      return NULL;
    }

  /* open-coded binary search */
  *index_entry = NULL;
  l = 0;
  h = self->index_size - 1;
  ofs = 0;
  while (l <= h)
    {
      m = (l+h) >> 1;
      mv = index_table[m].handle;
      if (mv == handle)
        {
          *index_entry = &index_table[m];
          ofs = index_table[m].ofs;
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

  NVEntry *entry = nv_table_get_entry_at_ofs(self, ofs);
  return entry;
}

static gboolean
nv_table_reserve_table_entry(NVTable *self, NVHandle handle, NVIndexEntry **index_entry)
{
  if (G_UNLIKELY(!(*index_entry) && !nv_table_is_handle_static(self, handle)))
    {
      /* this is a dynamic value */
      NVIndexEntry *index_table = nv_table_get_index(self);;
      gint l, h, m, ndx;
      gboolean found = FALSE;

      if (!nv_table_alloc_check(self, sizeof(index_table[0])))
        return FALSE;

      l = 0;
      h = self->index_size - 1;
      ndx = -1;
      while (l <= h)
        {
          guint16 mv;

          m = (l+h) >> 1;
          mv = index_table[m].handle;

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

      g_assert(ndx >= 0 && ndx <= self->index_size);
      if (ndx < self->index_size)
        {
          memmove(&index_table[ndx + 1], &index_table[ndx], (self->index_size - ndx) * sizeof(index_table[0]));
        }

      *index_entry = &index_table[ndx];

      /* we set ofs to zero here, which means that the NVEntry won't
         be found even if the slot is present in index */
      (**index_entry).handle = handle;
      (**index_entry).ofs    = 0;
      if (!found)
        self->index_size++;
    }
  return TRUE;
}

static inline void
nv_table_set_table_entry(NVTable *self, NVHandle handle, guint32 ofs, NVIndexEntry *index_entry)
{
  if (G_LIKELY(nv_table_is_handle_static(self, handle)))
    {
      /* this is a statically allocated value, simply store the offset */
      self->static_entries[handle-1] = ofs;
    }
  else
    {
      /* this is a dynamic value */
      (*index_entry).handle = handle;
      (*index_entry).ofs    = ofs;
    }
}

static gboolean
nv_table_make_direct(NVHandle handle, NVEntry *entry, NVIndexEntry *index_entry, gpointer user_data)
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
nv_table_add_value(NVTable *self, NVHandle handle, const gchar *name, gsize name_len, const gchar *value,
                   gsize value_len, gboolean *new_entry)
{
  NVEntry *entry;
  guint32 ofs;
  NVIndexEntry *index_entry;

  if (value_len > NV_TABLE_MAX_BYTES)
    value_len = NV_TABLE_MAX_BYTES;
  if (new_entry)
    *new_entry = FALSE;
  entry = nv_table_get_entry(self, handle, &index_entry);
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
  if (G_UNLIKELY(entry && (((guint) entry->alloc_len)) >= value_len + NV_ENTRY_DIRECT_HDR + name_len + 2))
    {
      gchar *dst;
      /* this value already exists and the new value fits in the old space */
      if (!entry->indirect)
        {
          dst = entry->vdirect.data + entry->name_len + 1;

          entry->vdirect.value_len = value_len;
          memmove(dst, value, value_len);
          dst[value_len] = 0;
        }
      else
        {
          /* this was an indirect entry, convert it */
          entry->indirect = 0;
          entry->vdirect.value_len = value_len;
          entry->name_len = name_len;
          memmove(entry->vdirect.data, name, name_len + 1);
          memmove(entry->vdirect.data + name_len + 1, value, value_len);
          entry->vdirect.data[entry->name_len + 1 + value_len] = 0;
        }
      entry->unset = FALSE;
      return TRUE;
    }
  else if (!entry && new_entry)
    *new_entry = TRUE;

  /* check if there's enough free space: size of the struct plus the
   * size needed for a dynamic table slot */
  if (!nv_table_reserve_table_entry(self, handle, &index_entry))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_DIRECT_HDR + name_len + value_len + 2);
  if (G_UNLIKELY(!entry))
    {
      return FALSE;
    }

  ofs = nv_table_get_ofs_for_an_entry(self, entry);
  entry->vdirect.value_len = value_len;
  if (handle >= self->num_static_entries)
    {
      /* we only store the name for non-builtin values */
      entry->name_len = name_len;
      memmove(entry->vdirect.data, name, name_len + 1);
    }
  else
    entry->name_len = 0;
  memmove(entry->vdirect.data + entry->name_len + 1, value, value_len);
  entry->vdirect.data[entry->name_len + 1 + value_len] = 0;

  nv_table_set_table_entry(self, handle, ofs, index_entry);
  return TRUE;
}

void
nv_table_unset_value(NVTable *self, NVHandle handle)
{
  NVIndexEntry *index_entry;
  NVEntry *entry = nv_table_get_entry(self, handle, &index_entry);

  if (!entry)
    return;
  entry->unset = TRUE;

  /* make sure the actual value is also set to the null_string just in case
   * this message is serialized and then deserialized by an earlier
   * syslog-ng version which does not support the unset flag */
  if (entry->indirect)
    {
      entry->vindirect.ofs = 0;
      entry->vindirect.len = 0;
    }
  else
    {
      entry->vdirect.value_len = 0;
      entry->vdirect.data[entry->name_len + 1] = 0;
    }
}

static void
nv_table_set_indirect_entry(NVTable *self, NVHandle handle, NVEntry *entry, const gchar *name, gsize name_len,
                            const NVReferencedSlice *referenced_slice)
{
  entry->vindirect.handle = referenced_slice->handle;
  entry->vindirect.ofs = referenced_slice->ofs;
  entry->vindirect.len = referenced_slice->len;
  entry->vindirect.type = referenced_slice->type;

  if (entry->indirect)
    return;

  /* previously a non-indirect entry, convert it */
  entry->indirect = 1;

  if (handle >= self->num_static_entries)
    {
      entry->name_len = name_len;
      memmove(entry->vindirect.name, name, name_len + 1);
    }
  else
    {
      entry->name_len = 0;
    }
}

static gboolean
nv_table_copy_referenced_value(NVTable *self, NVEntry *ref_entry, NVHandle handle, const gchar *name,
                               gsize name_len, NVReferencedSlice *ref_slice, gboolean *new_entry)
{

  gssize ref_length;
  const gchar *ref_value = nv_table_resolve_entry(self, ref_entry, &ref_length);

  if (ref_slice->ofs > ref_length)
    {
      ref_slice->len = 0;
      ref_slice->ofs = 0;
    }
  else
    {
      ref_slice->len = MIN(ref_slice->ofs + ref_slice->len, ref_length) - ref_slice->ofs;
    }

  return nv_table_add_value(self, handle, name, name_len, ref_value + ref_slice->ofs, ref_slice->len, new_entry);
}

gboolean
nv_table_add_value_indirect(NVTable *self, NVHandle handle, const gchar *name, gsize name_len,
                            NVReferencedSlice *referenced_slice, gboolean *new_entry)
{
  NVEntry *entry, *ref_entry;
  NVIndexEntry *index_entry;
  guint32 ofs;

  if (new_entry)
    *new_entry = FALSE;
  ref_entry = nv_table_get_entry(self, referenced_slice->handle, &index_entry);

  if ((ref_entry && ref_entry->indirect) || handle == referenced_slice->handle)
    {
      /* NOTE: uh-oh, the to-be-referenced value is already an indirect
       * reference, this is not supported, copy the stuff */
      return nv_table_copy_referenced_value(self, ref_entry, handle, name, name_len, referenced_slice, new_entry);
    }


  entry = nv_table_get_entry(self, handle, &index_entry);
  if ((!entry && !new_entry && referenced_slice->len == 0) || !ref_entry)
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

  if (entry && (((guint) entry->alloc_len) >= NV_ENTRY_INDIRECT_HDR + name_len + 1))
    {
      /* this value already exists and the new reference fits in the old space */
      nv_table_set_indirect_entry(self, handle, entry, name, name_len, referenced_slice);
      ref_entry->referenced = TRUE;
      return TRUE;
    }
  else if (!entry && new_entry)
    {
      *new_entry = TRUE;
    }

  if (!nv_table_reserve_table_entry(self, handle, &index_entry))
    return FALSE;
  entry = nv_table_alloc_value(self, NV_ENTRY_INDIRECT_HDR + name_len + 1);
  if (!entry)
    {
      return FALSE;
    }

  ofs = nv_table_get_ofs_for_an_entry(self, entry);

  nv_table_set_indirect_entry(self, handle, entry, name, name_len, referenced_slice);
  ref_entry->referenced = TRUE;

  nv_table_set_table_entry(self, handle, ofs, index_entry);

  return TRUE;
}

static gboolean
nv_table_call_foreach(NVHandle handle, NVEntry *entry, NVIndexEntry *index_entry, gpointer user_data)
{
  NVTable *self = (NVTable *) ((gpointer *) user_data)[0];
  NVRegistry *registry = (NVRegistry *) ((gpointer *) user_data)[1];
  NVTableForeachFunc func = ((gpointer *) user_data)[2];
  gpointer func_data = ((gpointer *) user_data)[3];
  const gchar *value;
  gssize value_len;

  if (entry->unset)
    return FALSE;
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
  NVIndexEntry *index_table;
  NVEntry *entry;
  gint i;

  for (i = 0; i < self->num_static_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, self->static_entries[i]);
      if (!entry)
        continue;

      if (func(i + 1, entry, NULL, user_data))
        return TRUE;
    }

  index_table = nv_table_get_index(self);
  for (i = 0; i < self->index_size; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, index_table[i].ofs);

      if (!entry)
        continue;

      if (func(index_table[i].handle, entry, &index_table[i], user_data))
        return TRUE;
    }

  return FALSE;
}

void
nv_table_init(NVTable *self, gsize alloc_length, gint num_static_entries)
{
  g_assert(alloc_length <= NV_TABLE_MAX_BYTES);
  self->size = alloc_length;
  self->used = 0;
  self->index_size = 0;
  self->num_static_entries = num_static_entries;
  self->ref_cnt = 1;
  self->borrowed = FALSE;
  memset(&self->static_entries[0], 0, self->num_static_entries * sizeof(self->static_entries[0]));
}

NVTable *
nv_table_new(gint num_static_entries, gint index_size_hint, gint init_length)
{
  NVTable *self;
  gsize alloc_length;

  alloc_length = nv_table_get_alloc_size(num_static_entries, index_size_hint, init_length);
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

/* returns TRUE if successfully realloced, FALSE means that we're unable to grow */
gboolean
nv_table_realloc(NVTable *self, NVTable **new)
{
  gsize old_size = self->size;
  gsize new_size;

  /* double the size of the current allocation */
  new_size = ((gsize) self->size) << 1;
  if (new_size > NV_TABLE_MAX_BYTES)
    new_size = NV_TABLE_MAX_BYTES;
  if (new_size == old_size)
    return FALSE;

  if (self->ref_cnt == 1 && !self->borrowed)
    {
      *new = self = g_realloc(self, new_size);

      self->size = new_size;
      /* move the downwards growing region to the end of the new buffer */
      memmove(NV_TABLE_ADDR(self, self->size - self->used),
              NV_TABLE_ADDR(self, old_size - self->used),
              self->used);
    }
  else
    {
      *new = g_malloc(new_size);

      /* we only copy the header first */
      memcpy(*new, self, sizeof(NVTable) + self->num_static_entries * sizeof(self->static_entries[0]) + self->index_size *
             sizeof(NVIndexEntry));
      (*new)->ref_cnt = 1;
      (*new)->borrowed = FALSE;
      (*new)->size = new_size;

      memmove(NV_TABLE_ADDR((*new), (*new)->size - (*new)->used),
              NV_TABLE_ADDR(self, old_size - self->used),
              self->used);

      nv_table_unref(self);
    }
  return TRUE;
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
    new_size = self->size + (NV_TABLE_BOUND(additional_space));

  if (new_size > NV_TABLE_MAX_BYTES)
    new_size = NV_TABLE_MAX_BYTES;

  new = g_malloc(new_size);
  memcpy(new, self, sizeof(NVTable) + self->num_static_entries * sizeof(self->static_entries[0]) + self->index_size *
         sizeof(NVIndexEntry));
  new->size = new_size;
  new->ref_cnt = 1;
  new->borrowed = FALSE;

  memcpy(NV_TABLE_ADDR(new, new->size - new->used),
         NV_TABLE_ADDR(self, self->size - self->used),
         self->used);

  return new;
}
