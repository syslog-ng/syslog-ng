/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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
#include "nvtable-serialize-endianutils.h"
#include "logmsg/logmsg.h"

#define NV_TABLE_MAGIC_V2  "NVT2"
#define NVT_SF_BE           0x1


typedef struct _NVTableMetaData
{
  guint32 magic;
  guint8 flags;
} NVTableMetaData;

static gint
_dyn_entry_cmp(const void *a, const void *b)
{
  NVDynValue entry_a = *(NVDynValue *)a;
  NVDynValue entry_b = *(NVDynValue *)b;
  NVHandle handle_a = NV_TABLE_DYNVALUE_HANDLE(entry_a);
  NVHandle handle_b = NV_TABLE_DYNVALUE_HANDLE(entry_b);

  if (handle_a == handle_b)
    {
      return 0;
    }

  return (handle_a < handle_b) ? -1 : 1;
}

void
_update_indirect_dynamic_entry(NVTable* self, NVHandle oh, const gchar* name, NVEntry* entry, NVRegistry *logmsg_nv_registry)
{
  NVDynValue* dyn_slot;
  NVEntry* oe = nv_table_get_entry(self, oh, &dyn_slot);
  const gchar* oname = nv_entry_get_name(oe);
  if (G_UNLIKELY(oname[0] == '\0'))
    {
      msg_debug("nvtable: the name of the nventry referred by entry->indirect has zero length",
                evt_tag_str("entry.name", name),
                NULL);
    }
  else
    {
      oh = nv_registry_alloc_handle(logmsg_nv_registry, oname);
      if (oh != 0)
        entry->vindirect.handle = oh;
    }
}

static void
_update_indirect_entry(NVTable *self, NVRegistry *logmsg_nv_registry,
                       NVEntry* entry)
{
  NVHandle oh = entry->vindirect.handle;
  const gchar *name = nv_entry_get_name(entry);

  if (oh > self->num_static_entries)
    {
      _update_indirect_dynamic_entry(self, oh, name, entry, logmsg_nv_registry);
    }
  nv_registry_alloc_handle(logmsg_nv_registry, name);
}

static inline gboolean
_is_indirect(NVEntry *entry)
{
  return entry && entry->indirect;
}

static void
_update_all_indirect_entries(NVTable *self, NVRegistry *logmsg_nv_registry)
{
  int i;
  NVDynValue *dyn_entries = nv_table_get_dyn_entries(self);

  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVEntry *entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));

      if (_is_indirect(entry))
        _update_indirect_entry(self, logmsg_nv_registry, entry);
    }
}

void
_copy_handles(NVHandle* handles_to_update, NVHandle* new_updated_handles, guint8 num_handles_to_update)
{
  guint16 i;
  for (i = 0; i < num_handles_to_update; i++)
    {
      handles_to_update[i] = new_updated_handles[i];
    }
}

const gchar *
_get_entry_name(NVTable *self, NVDynValue *dyn_entry)
{
  NVEntry *entry = nv_table_get_entry_at_ofs(self, dyn_entry->ofs);
  if (!entry)
    return NULL;
  return nv_entry_get_name(entry);
}

static void
_set_updated_handle(NVDynValue *dyn_entry, NVHandle new_handle, NVHandle *handles_to_update, NVHandle *updated_handles, guint8 num_handles_to_update)
{
  NVHandle old_handle = dyn_entry->handle;
  guint8 j;
  dyn_entry->handle = new_handle;
  if (handles_to_update)
    {
      for (j = 0; j < num_handles_to_update; j++)
        {
          if (handles_to_update[j] == old_handle)
            {
              updated_handles[j] = new_handle;
            }
        }
    }
}

static void
_update_dynamic_handles(NVTable *self, NVRegistry *logmsg_nv_registry,
                        NVHandle *handles_to_update,
                        guint8 num_handles_to_update)
{
  guint16 i;
  NVHandle *updated_handles = g_new0(NVHandle, num_handles_to_update);
  NVDynValue* dyn_entries = nv_table_get_dyn_entries(self);

  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVHandle new_handle;
      NVDynValue *dyn_entry = &dyn_entries[i];
      const gchar *name = _get_entry_name(self, dyn_entry);

      if (!name)
        continue;

      new_handle = nv_registry_alloc_handle(logmsg_nv_registry, name);

      _set_updated_handle(dyn_entry, new_handle, handles_to_update, updated_handles, num_handles_to_update);
    }

  _copy_handles(handles_to_update, updated_handles, num_handles_to_update);

  qsort(dyn_entries, self->num_dyn_entries, sizeof(NVDynValue), _dyn_entry_cmp);
  g_free(updated_handles);
}

/**
 * serialize / deserialize functions
 **/

static gboolean
_update_sd_entries(NVHandle handle, const gchar *name, const gchar *value, gssize value_len, gpointer user_data)
{
  if (strncmp(name, logmsg_sd_prefix, logmsg_sd_prefix_len) == 0 && name[6])
    {
      guint16 flag;
      gchar *dot;
      dot = strrchr(name,'.');
      if (dot - name - logmsg_sd_prefix_len < 0)
        {
          /* Standalone SDATA */
          flag = ((strlen(name) - logmsg_sd_prefix_len) << 8) + LM_VF_SDATA;
        }
      else
        {
          flag = ((dot - name - logmsg_sd_prefix_len) << 8) + LM_VF_SDATA;
        }
      nv_registry_set_handle_flags(logmsg_registry, handle, flag);
    }
  return FALSE;
}

void
nv_table_update_handles(NVTable *self, NVRegistry *logmsg_nv_registry,
                    NVHandle *handles_to_update, guint8 num_handles_to_update)
{
  _update_all_indirect_entries(self, logmsg_nv_registry);
  _update_dynamic_handles(self, logmsg_nv_registry, handles_to_update, num_handles_to_update);
  if (handles_to_update)
    {
      nv_table_foreach(self, logmsg_nv_registry, _update_sd_entries, NULL);
    }
}

static gboolean
_deserialize_dyn_value(SerializeArchive *sa, NVDynValue* dyn_value)
{
  return serialize_read_nvhandle(sa, &(dyn_value->handle)) &&
         serialize_read_uint32(sa, &(dyn_value->ofs));
};

static gboolean
_deserialize_static_entries(SerializeArchive *sa, NVTable *res)
{
  guint32 i;
  for (i = 0; i < res->num_static_entries; i++)
     {
       if (!serialize_read_uint32(sa, &res->static_entries[i]))
         return FALSE;
     }
  return TRUE;
}

static gboolean
_deserialize_dynamic_entries(SerializeArchive *sa, NVTable *res)
{
  guint32 i;
  NVDynValue *dyn_entries;
  dyn_entries = nv_table_get_dyn_entries(res);
  for (i = 0; i < res->num_dyn_entries; i++)
    {
      if (!_deserialize_dyn_value(sa, &dyn_entries[i]))
        return FALSE;
    }
  return TRUE;
}

static gboolean
_read_struct(SerializeArchive *sa, NVTable *res)
{
  return _deserialize_static_entries(sa, res) && _deserialize_dynamic_entries(sa, res);
}

static gboolean
_has_to_swap_bytes(guint8 flags)
{
  return !!(flags & NVT_SF_BE) != (G_BYTE_ORDER == G_BIG_ENDIAN);
}

static inline gboolean
_read_magic(SerializeArchive *sa, guint32 *magic)
{
  return serialize_read_uint32(sa, magic);
}

static inline gboolean
_read_flags(SerializeArchive *sa, guint8 *flags)
{
  return serialize_read_uint8(sa, flags);
}

static inline gboolean
_read_metadata(SerializeArchive *sa, NVTableMetaData *meta_data)
{
  if (!_read_magic(sa, &meta_data->magic))
    {
      return FALSE;
    }

  if (!_read_flags(sa, &meta_data->flags))
    {
      return FALSE;
    }

  if (_has_to_swap_bytes(meta_data->flags))
    {
      meta_data->magic = GUINT32_SWAP_LE_BE(meta_data->magic);
    }

  if (memcmp((void *)&meta_data->magic, (const void *)NV_TABLE_MAGIC_V2, 4) != 0)
    {
      return FALSE;
    }
  return TRUE;
}

static gboolean
_read_header(SerializeArchive *sa, NVTable **nvtable)
{
  g_assert(*nvtable == NULL);

  NVTable *res = (NVTable *)g_malloc(sizeof(NVTable));
  if (!serialize_read_uint32(sa, &res->size))
    {
      goto error;
    }
  if (!serialize_read_uint32(sa, &res->used))
    {
      goto error;
    }
  if (!serialize_read_uint16(sa, &res->num_dyn_entries))
    {
      goto error;
    }
  if (!serialize_read_uint8(sa, &res->num_static_entries))
    {
      goto error;
    }

  res = (NVTable *)g_realloc(res, res->size);
  res->borrowed = FALSE;
  res->ref_cnt = 1;
  *nvtable = res;
  return TRUE;

error:
  if (res)
    g_free(res);
  return FALSE;
}

static inline gboolean
_read_payload(SerializeArchive *sa, NVTable *res)
{
  return serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), res->used);
}

NVTable *
nv_table_deserialize(SerializeArchive *sa)
{
  NVTableMetaData meta_data;
  NVTable *res = NULL;

  if (!_read_metadata(sa, &meta_data))
    {
      goto error;
    }

  if (!_read_header(sa, &res))
    {
      goto error;
    }

  if (!_read_struct(sa, res))
    {
      goto error;
    }

  if (!_read_payload(sa, res))
    {
      goto error;
    }

  if (_has_to_swap_bytes(meta_data.flags))
    {
      nv_table_data_swap_bytes(res);
    }

  return res;

error:
  if (res)
    g_free(res);
  return NULL;
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

static void
_write_meta_data(SerializeArchive *sa, NVTableMetaData *meta_data)
{
  serialize_write_uint32(sa, meta_data->magic);
  serialize_write_uint8(sa, meta_data->flags);
}

static void
_fill_meta_data(NVTable *self, NVTableMetaData *meta_data)
{
  memcpy((void *)&meta_data->magic,(const void *) NV_TABLE_MAGIC_V2, 4);
  if (G_BYTE_ORDER == G_BIG_ENDIAN)
     meta_data->flags |= NVT_SF_BE;
}

static void
_write_payload(SerializeArchive *sa, NVTable *self)
{
  serialize_write_blob(sa, NV_TABLE_ADDR(self, self->size - self->used), self->used);
}

gboolean
nv_table_serialize(SerializeArchive *sa, NVTable *self)
{
  NVTableMetaData meta_data = {0};

  _fill_meta_data(self, &meta_data);
  _write_meta_data(sa, &meta_data);

  _write_struct(sa, self);

  _write_payload(sa, self);
  return TRUE;
}
