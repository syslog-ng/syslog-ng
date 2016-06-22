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

#include "logmsg/nvtable-serialize.h"
#include "logmsg/nvtable-serialize-endianutils.h"
#include "logmsg/logmsg.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>

#define NV_TABLE_MAGIC_V2  "NVT2"
#define NVT_SF_BE           0x1

static gboolean
serialize_read_nvhandle(SerializeArchive *sa, NVHandle* handle)
{
  return serialize_read_uint32(sa, handle);
}

static gboolean
serialize_write_nvhandle(SerializeArchive *sa, NVHandle handle)
{
  return serialize_write_uint32(sa, handle);
}

typedef struct _NVTableMetaData
{
  guint32 magic;
  guint8 flags;
} NVTableMetaData;

/**********************************************************************
 * This chunk of code fixes up the NVHandle values scattered in a
 * deserialized NVTable.
 *
 * The reason they need fixing is that NVHandles are allocated dynamically
 * when they are first used in a syslog-ng process. As the serialized
 * representation of a LogMessage can be read back by another syslog-ng
 * process, its idea of the name-value pair handle might be different.
 *
 * This means that we need to iterate through the struct and change the
 * handle values. This is not even a simple operation as handles are embedded
 * in various locations
 *   - in the dynamic entry table, an array sorted by handle
 *   - as indirect values that refer to other values
 *   - the SDATA handles array that ensures that SDATA values are ordered
 *     the same way they were received.
 *
 **********************************************************************/

static gint
_dyn_entry_cmp(const void *a, const void *b)
{
  NVDynValue *entry_a = (NVDynValue *) a;
  NVDynValue *entry_b = (NVDynValue *) b;
  NVHandle handle_a = entry_a->handle;
  NVHandle handle_b = entry_b->handle;

  if (handle_a == handle_b)
    {
      return 0;
    }

  return (handle_a < handle_b) ? -1 : 1;
}

static void
_copy_sdata_handles(LogMessageSerializationState *state)
{
  memcpy(state->msg->sdata, state->updated_sdata_handles, sizeof(state->msg->sdata[0]) * state->msg->num_sdata);
}

static void
_copy_dyn_entries(LogMessageSerializationState *state)
{
  NVTable *self = state->msg->payload;

  memmove(nv_table_get_dyn_entries(self), state->updated_dyn_values, sizeof(NVDynValue) * self->num_dyn_entries);
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
_set_updated_handle(NVHandle old_handle, NVHandle new_handle, NVHandle *handles_to_update, NVHandle *updated_handles, guint8 num_handles_to_update)
{
  guint8 j;

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
_fixup_handle_in_dynamic_entry(LogMessageSerializationState *state, NVDynValue *dyn_entry, NVHandle new_handle)
{
  NVDynValue *new_dyn_entry = &state->updated_dyn_values[state->cur_dyn_value++];
  const gchar *name = _get_entry_name(state->msg->payload, dyn_entry);

  if (!name)
    return;

  *new_dyn_entry = *dyn_entry;
  new_dyn_entry->handle = new_handle;
  _set_updated_handle(dyn_entry->handle, new_handle, state->msg->sdata, state->updated_sdata_handles, state->msg->num_sdata);
}

static void
_sort_dynamic_handles(LogMessageSerializationState *state)
{
  NVTable *self = state->msg->payload;

  qsort(state->updated_dyn_values, self->num_dyn_entries, sizeof(NVDynValue), _dyn_entry_cmp);
}

static NVHandle
_allocate_handle_for_entry_name(NVHandle old_handle, NVEntry *entry)
{
  const gchar *name = nv_entry_get_name(entry);

  if (entry->name_len != 0)
    return log_msg_get_value_handle(name);
  else
    return old_handle;
}

static NVHandle
_allocate_handle_of_referenced_entry(NVTable *self, NVHandle ref_handle)
{
  NVDynValue* dyn_slot;
  NVEntry *ref_entry = nv_table_get_entry(self, ref_handle, &dyn_slot);

  return _allocate_handle_for_entry_name(ref_handle, ref_entry);
}

static inline gboolean
_is_indirect(NVEntry *entry)
{
  return entry && entry->indirect;
}

static void
_fixup_handle_in_indirect_entry(NVTable *self, NVEntry *entry)
{
  if (_is_indirect(entry))
    entry->vindirect.handle = _allocate_handle_of_referenced_entry(self, entry->vindirect.handle);
}

static gboolean
_fixup_entry(NVHandle handle, NVEntry *entry, NVDynValue *dyn_value, gpointer user_data)
{
  LogMessageSerializationState *state = (LogMessageSerializationState *) user_data;
  NVTable *self = state->msg->payload;
  NVHandle new_handle;

  new_handle = _allocate_handle_for_entry_name(handle, entry);
  _fixup_handle_in_indirect_entry(self, entry);

  if (dyn_value)
    _fixup_handle_in_dynamic_entry(state, dyn_value, new_handle);
              
  return FALSE;
}

void
nv_table_fixup_handles(LogMessageSerializationState *state)
{
  NVTable *self = state->msg->payload;

  state->updated_sdata_handles = g_new0(NVHandle, state->msg->num_sdata);
  state->updated_dyn_values = g_new0(NVDynValue, state->msg->payload->num_dyn_entries);
  state->cur_dyn_value = 0;

  nv_table_foreach_entry(self, _fixup_entry, state);

  _copy_sdata_handles(state);
  _sort_dynamic_handles(state);
  _copy_dyn_entries(state);

  g_free(state->updated_sdata_handles);
  state->updated_sdata_handles = NULL;
  g_free(state->updated_dyn_values);
  state->updated_dyn_values = NULL;
}

/**********************************************************************
 * deserialize an NVTable
 **********************************************************************/

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
nv_table_deserialize(LogMessageSerializationState *state)
{
  SerializeArchive *sa = state->sa;
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

/**********************************************************************
 * serialize an NVTable
 **********************************************************************/


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
nv_table_serialize(LogMessageSerializationState *state, NVTable *self)
{
  NVTableMetaData meta_data = { 0 };
  SerializeArchive *sa = state->sa;

  _fill_meta_data(self, &meta_data);
  _write_meta_data(sa, &meta_data);

  _write_struct(sa, self);

  _write_payload(sa, self);
  return TRUE;
}
