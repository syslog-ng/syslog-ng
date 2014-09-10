/*
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */


#include "nvtable-serialize.h"
#include "messages.h"

#include <stdlib.h>

/**
 * serialize / unserialize functions
 **/
static gint
dyn_entry_cmp(const void *a, const void *b)
{
  guint32 entry_a = *(guint32 *)a;
  guint32 entry_b = *(guint32 *)b;
  NVHandle handle_a = NV_TABLE_DYNVALUE_HANDLE(entry_a);
  NVHandle handle_b = NV_TABLE_DYNVALUE_HANDLE(entry_b);
  return (handle_a - handle_b);
}

void
nv_table_update_ids(NVTable *self,NVRegistry *logmsg_registry, NVHandle *handles_to_update, guint8 num_handles_to_update)
{
  guint32 *dyn_entries = nv_table_get_dyn_entries(self);
  guint16 i,j;
  NVHandle *new_updated_handles = g_new0(NVHandle, num_handles_to_update);

  /*
     upgrade indirect entries:
     - get parent the handle from the entry
     - get the parent entry
     - if parent entry is static (predefined) skip the following steps
     - get the name of the parent entry handle
     - allocate the name -> get a handle
     - set the given handle for the entry
  */
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVEntry *entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));
      if (!entry)
        continue;
      if (entry->indirect)
        {
          NVHandle oh = entry->vindirect.handle;
          const gchar *name = nv_entry_get_name(entry);
          if (oh > self->num_static_entries)
            {
              /* Only do this refresh on non-static entries */
              guint32 *dyn_slot;
              NVEntry *oe = nv_table_get_entry(self,oh,&dyn_slot);
              const gchar *oname = nv_entry_get_name(oe);
              if (G_UNLIKELY(oname[0] == '\0'))
                 msg_debug ("nvtable: entry->indirect => name is zero length",
                            evt_tag_str("entry.name", name),
                            evt_tag_int("i", i),
                            NULL);
              else
                {
                  oh = nv_registry_alloc_handle(logmsg_registry,oname);
                  if (oh != 0)
                    entry->vindirect.handle = oh;
                }
            }
          nv_registry_alloc_handle(logmsg_registry, name);
        }
    }

  /*
     upgrade handles:
  */
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVHandle handle;
      NVHandle old_handle;
      NVEntry *entry;
      const gchar *name;
      guint16 ofs;

      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));
      old_handle = NV_TABLE_DYNVALUE_HANDLE(dyn_entries[i]);
      name = nv_entry_get_name(entry);
      if (!entry)
        continue;
      handle = nv_registry_alloc_handle(logmsg_registry, name);
      ofs = NV_TABLE_DYNVALUE_OFS(dyn_entries[i]);
      dyn_entries[i] = (handle << 16) + ofs;
      if (handles_to_update)
        {
          for (j = 0; j < num_handles_to_update; j++)
            {
              if (handles_to_update[j] == old_handle)
                {
                  new_updated_handles[j] = handle;
                }
            }
        }
    }
  for (i = 0; i < num_handles_to_update; i++)
    {
      handles_to_update[i] =  new_updated_handles[i];
    }
  qsort(dyn_entries, self->num_dyn_entries, sizeof(guint32), dyn_entry_cmp);
  g_free(new_updated_handles);
  return;
}

static inline guint8 reverse(guint8 b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void
nv_table_swap_entry_flags(NVEntry *entry)
{
  entry->flags = reverse(entry->flags);
}

static void
nv_entry_swap_bytes(NVEntry *entry)
{
  nv_table_swap_entry_flags(entry);
  entry->alloc_len = GUINT16_SWAP_LE_BE(entry->alloc_len);
  if (!entry->indirect)
    {
      entry->vdirect.value_len = GUINT16_SWAP_LE_BE(entry->vdirect.value_len);
    }
  else
    {
      entry->vindirect.handle = GUINT16_SWAP_LE_BE(entry->vindirect.handle);
      entry->vindirect.ofs = GUINT16_SWAP_LE_BE(entry->vindirect.ofs);
      entry->vindirect.len = GUINT16_SWAP_LE_BE(entry->vindirect.len);
    }
}


void
nv_table_data_swap_bytes(NVTable *self)
{
  guint32 *dyn_entries;
  NVEntry *entry;
  gint i;
  for (i = 0; i < self->num_static_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, self->static_entries[i]);
      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }

  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));

      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }
}

void
nv_table_struct_swap_bytes(NVTable *self)
{
  guint16 i;
  guint32 *dyn_entries;
  self->size = GUINT16_SWAP_LE_BE(self->size);
  self->used = GUINT16_SWAP_LE_BE(self->used);
  self->num_dyn_entries = GUINT16_SWAP_LE_BE(self->num_dyn_entries);
  for (i = 0; i < self->num_static_entries; i++)
    {
      self->static_entries[i] = GUINT16_SWAP_LE_BE(self->static_entries[i]);
    }
  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      dyn_entries[i] = GUINT32_SWAP_LE_BE(dyn_entries[i]);
    }
}

gboolean
nv_table_unserialize_struct(SerializeArchive *sa, NVTable *res)
{
  guint16 i;
  guint32 *dyn_entries;
  for (i = 0; i < res->num_static_entries; i++)
    {
      if (!serialize_read_uint16(sa, &res->static_entries[i]))
        return FALSE;
    }
  dyn_entries = nv_table_get_dyn_entries(res);
  for (i = 0; i < res->num_dyn_entries; i++)
    {
      if (!serialize_read_uint32(sa, &dyn_entries[i]))
        return FALSE;
    }
  return TRUE;
}

NVTable *
nv_table_unserialize_22(SerializeArchive *sa)
{
  guint32 magic = 0;
  guint8 flags = 0;
  NVTable *res = NULL;
  guint32 used_len = 0;
  gboolean is_big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  if (!serialize_read_uint32(sa, &magic))
    return NULL;
  if (!serialize_read_uint8(sa, &flags))
    return NULL;
  if (!!(flags & NVT_SF_BE) != is_big_endian)
    {
      magic = GUINT32_SWAP_LE_BE(magic);
    }
  if (memcmp((void *)&magic, (const void *)NV_TABLE_MAGIC_V2, 4)!=0)
    return NULL;
  res = (NVTable *)g_malloc(sizeof(NVTable));
  if (!serialize_read_uint16(sa, &res->size))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint16(sa, &res->used))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint16(sa, &res->num_dyn_entries))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint8(sa, &res->num_static_entries))
    {
      g_free(res);
      return NULL;
    }
  res->ref_cnt = 1;
  res = (NVTable *)g_realloc(res,res->size << NV_TABLE_SCALE);
  res->borrowed = FALSE;
  if(!res)
    {
      return NULL;
    }
  if (!nv_table_unserialize_struct(sa, res))
    {
      g_free(res);
      return NULL;
    }
  used_len = res->used << NV_TABLE_SCALE;
  if (!serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), used_len))
    {
      g_free(res);
      return NULL;
    }
  if (is_big_endian != (flags & NVT_SF_BE))
    {
      nv_table_data_swap_bytes(res);
    }
  return res;
}

NVTable *
nv_table_unserialize(SerializeArchive *sa, guint8 log_msg_version)
{
  guint32 header_len=0;
  guint32 calculated_header_len=0;
  guint32 calculated_used_len=0;
  guint32 used_len=0;
  NVTable *res;
  gboolean swap_bytes=FALSE;
  if (log_msg_version < 22)
    {
      if (!serialize_read_uint32(sa, &header_len))
        return NULL;
      res = (NVTable *)g_try_malloc(header_len);
      if (!res)
        return NULL;
      if (!serialize_read_blob(sa, res, header_len))
        {
          g_free(res);
          return NULL;
        }
      calculated_header_len = sizeof(NVTable) + res->num_static_entries * sizeof(res->static_entries[0]) + res->num_dyn_entries * sizeof(guint32);
      if (!serialize_read_uint32(sa, &used_len))
        {
          g_free(res);
          return NULL;
        }
      calculated_used_len = res->used << NV_TABLE_SCALE;
      if (calculated_used_len != used_len || calculated_header_len != header_len)
        swap_bytes=TRUE;

      if (swap_bytes)
        nv_table_struct_swap_bytes(res);

      res = (NVTable *)g_try_realloc(res,res->size << NV_TABLE_SCALE);
      if (!res)
        return NULL;
      res->borrowed = FALSE;
      res->ref_cnt = 1;
      if (!serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), used_len))
        {
          g_free(res);
          return NULL;
        }

      if (swap_bytes)
        nv_table_data_swap_bytes(res);
    }
  else
    {
      res = nv_table_unserialize_22(sa);
    }
  return res;
}

void
nv_table_write_struct(SerializeArchive *sa, NVTable *self)
{
  guint16 i;
  guint32 *dyn_entries;
  serialize_write_uint16(sa, self->size);
  serialize_write_uint16(sa, self->used);
  serialize_write_uint16(sa, self->num_dyn_entries);
  serialize_write_uint8(sa, self->num_static_entries);
  for (i = 0; i < self->num_static_entries; i++)
    {
      serialize_write_uint16(sa, self->static_entries[i]);
    }
  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      serialize_write_uint32(sa, dyn_entries[i]);
    }
}

gboolean
nv_table_serialize(SerializeArchive *sa,NVTable *self)
{
  guint8 serialized_flag = 0;
  guint32 magic = 0;
  guint32 used_len;
  memcpy((void *)&magic,(const void *) NV_TABLE_MAGIC_V2, 4);
  serialize_write_uint32(sa, magic);
  if (G_BYTE_ORDER == G_BIG_ENDIAN)
    serialized_flag |= NVT_SF_BE;
  serialize_write_uint8(sa, serialized_flag);
  nv_table_write_struct(sa, self);
  used_len = self->used << NV_TABLE_SCALE;
  serialize_write_blob(sa, NV_TABLE_ADDR(self, self->size - self->used), used_len);
  return TRUE;
}
