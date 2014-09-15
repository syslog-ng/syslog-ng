/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2013 Viktor Juhasz
 * Copyright (c) 2012-2013 Viktor Tusa
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

#include "nvtable-serialize.h"
#include "messages.h"
#include "stdlib.h"
#include "string.h"
#include "nvtable-serialize-legacy.h"
#include "nvtable-serialize-endianutils.h"

#define NV_TABLE_MAGIC_V2  "NVT2"
#define NVT_SF_BE           0x1

static gint
_dyn_entry_cmp(const void *a, const void *b)
{
  NVDynValue entry_a = *(NVDynValue *)a;
  NVDynValue entry_b = *(NVDynValue *)b;
  NVHandle handle_a = NV_TABLE_DYNVALUE_HANDLE(entry_a);
  NVHandle handle_b = NV_TABLE_DYNVALUE_HANDLE(entry_b);

  return (handle_a - handle_b);
}

static void
_update_indirect_entry(NVTable *self, NVRegistry *logmsg_registry,
                       NVEntry* entry)
{
  NVHandle oh = entry->vindirect.handle;
  const gchar *name = nv_entry_get_name(entry);

  if (oh > self->num_static_entries)
    {
      /* Only do this refresh on non-static entries */
      NVDynValue *dyn_slot;
      NVEntry *oe = nv_table_get_entry(self, oh, &dyn_slot);
      const gchar *oname = nv_entry_get_name(oe);

      if (G_UNLIKELY(oname[0] == '\0'))
        msg_debug ("nvtable: the name of the nventry referred by entry->indirect has zero length",
                   evt_tag_str("entry.name", name),
                   NULL);
      else
        {
          oh = nv_registry_alloc_handle(logmsg_registry, oname);
          if (oh != 0)
            entry->vindirect.handle = oh;
        }
    }
  nv_registry_alloc_handle(logmsg_registry, name);
}

static void
_update_all_indirect_entries(NVTable *self, NVRegistry *logmsg_registry)
{
  int i;
  NVDynValue *dyn_entries = nv_table_get_dyn_entries(self);

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
      NVEntry *entry =
        nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));

      if (!entry)
        continue;

      if (entry->indirect)
        _update_indirect_entry(self, logmsg_registry, entry);
    }
}

static void
_update_dynamic_handles(NVTable *self, NVRegistry *logmsg_registry,
                        NVHandle *handles_to_update,
                        guint8 num_handles_to_update)
{
  guint16 i, j;
  NVHandle *new_updated_handles = g_new0(NVHandle, num_handles_to_update);
  NVDynValue* dyn_entries = nv_table_get_dyn_entries(self);

  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVHandle handle;
      NVHandle old_handle;
      NVEntry *entry;
      const gchar *name;
      guint32 ofs;

      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));
      old_handle = NV_TABLE_DYNVALUE_HANDLE(dyn_entries[i]);
      name = nv_entry_get_name(entry);
      if (!entry)
        continue;

      handle = nv_registry_alloc_handle(logmsg_registry, name);
      ofs = nv_table_get_dyn_value_offset_from_nventry(self, entry);
      dyn_entries[i].handle = handle;
      dyn_entries[i].ofs = ofs;

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
      handles_to_update[i] = new_updated_handles[i];
    }

  qsort(dyn_entries, self->num_dyn_entries, sizeof(NVDynValue), _dyn_entry_cmp);
  g_free(new_updated_handles);
}

/**
 * serialize / deserialize functions
 **/

void
nv_table_update_ids(NVTable *self, NVRegistry *logmsg_registry,
                    NVHandle *handles_to_update, guint8 num_handles_to_update)
{
  _update_all_indirect_entries(self, logmsg_registry);
  _update_dynamic_handles(self, logmsg_registry,
                          handles_to_update, num_handles_to_update);
  return;
}

static gboolean
_deserialize_dyn_value(SerializeArchive *sa, NVDynValue* dyn_value)
{
  return serialize_read_nvhandle(sa, &(dyn_value->handle)) &&
         serialize_read_uint32(sa, &(dyn_value->ofs));
};

static gboolean
_deserialize_struct_v25(SerializeArchive *sa, NVTable *res)
{
  guint32 i;
  NVDynValue *dyn_entries;

  for (i = 0; i < res->num_static_entries; i++)
    {
      if (!serialize_read_uint32(sa, &res->static_entries[i]))
        return FALSE;
    }

  dyn_entries = nv_table_get_dyn_entries(res);
  for (i = 0; i < res->num_dyn_entries; i++)
    {
      if (!_deserialize_dyn_value(sa, &dyn_entries[i]))
        return FALSE;
    }

  return TRUE;
}

NVTable *
nv_table_deserialize_newest(SerializeArchive *sa)
{
  guint32 magic = 0;
  guint8 flags = 0;
  NVTable *res = NULL;
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
  if (!serialize_read_uint32(sa, &res->size))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint32(sa, &res->used))
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

  res = (NVTable *)g_realloc(res,res->size);
  if(!res)
    {
      return NULL;
    }

  res->borrowed = FALSE;
  res->ref_cnt = 1;
  if (!_deserialize_struct_v25(sa, res))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), res->used))
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
nv_table_deserialize(SerializeArchive *sa, guint8 log_msg_version)
{
  NVTable *res;

  if (log_msg_version < 22)
    {
      res = nv_table_deserialize_legacy(sa);
    }
  else if (log_msg_version < 26)
    {
      res = nv_table_deserialize_22(sa);
    }
  else
    {
      res = nv_table_deserialize_newest(sa);
    }
  return res;
}

static void
_serialize_nv_dyn_value(SerializeArchive *sa, NVDynValue *dyn_value)
{
  serialize_write_nvhandle(sa, dyn_value->handle);
  serialize_write_uint32(sa, dyn_value->ofs);
};

static void
_write_struct(SerializeArchive *sa, NVTable *self)
{
  guint16 i;
  NVDynValue *dyn_entries;

  serialize_write_uint32(sa, self->size);
  serialize_write_uint32(sa, self->used);
  serialize_write_uint16(sa, self->num_dyn_entries);
  serialize_write_uint8(sa, self->num_static_entries);
  for (i = 0; i < self->num_static_entries; i++)
    {
      serialize_write_uint32(sa, self->static_entries[i]);
    }
  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      _serialize_nv_dyn_value(sa, &dyn_entries[i]);
    }
}

gboolean
nv_table_serialize(SerializeArchive *sa, NVTable *self)
{
  guint8 serialized_flag = 0;
  guint32 magic = 0;
  guint32 used_len;

  memcpy((void *)&magic,(const void *) NV_TABLE_MAGIC_V2, 4);
  serialize_write_uint32(sa, magic);

  if (G_BYTE_ORDER == G_BIG_ENDIAN)
    serialized_flag |= NVT_SF_BE;
  serialize_write_uint8(sa, serialized_flag);

  _write_struct(sa, self);

  used_len = self->used;
  serialize_write_blob(sa, NV_TABLE_ADDR(self, self->size - self->used), used_len);
  return TRUE;
}
