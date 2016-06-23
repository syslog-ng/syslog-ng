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
 *   - in the index table, an array sorted by handle
 *   - as indirect values that refer to other values
 *   - the SDATA handles array that ensures that SDATA values are ordered
 *     the same way they were received.
 *
 **********************************************************************/

static gint
_index_entry_cmp(const void *a, const void *b)
{
  NVIndexEntry *entry_a = (NVIndexEntry *) a;
  NVIndexEntry *entry_b = (NVIndexEntry *) b;
  NVHandle handle_a = entry_a->handle;
  NVHandle handle_b = entry_b->handle;

  if (handle_a == handle_b)
    {
      return 0;
    }

  return (handle_a < handle_b) ? -1 : 1;
}

static void
_copy_updated_sdata_handles(LogMessageSerializationState *state)
{
  memcpy(state->msg->sdata, state->updated_sdata_handles, sizeof(state->msg->sdata[0]) * state->msg->num_sdata);
}

static void
_sort_updated_index(LogMessageSerializationState *state)
{
  NVTable *self = state->msg->payload;

  qsort(state->updated_index, self->index_size, sizeof(NVIndexEntry), _index_entry_cmp);
}

static void
_copy_updated_index(LogMessageSerializationState *state)
{
  NVTable *self = state->msg->payload;

  memmove(nv_table_get_index(self), state->updated_index, sizeof(NVIndexEntry) * self->index_size);
}

const gchar *
_get_entry_name(NVTable *self, NVIndexEntry *index_entry)
{
  NVEntry *entry = nv_table_get_entry_at_ofs(self, index_entry->ofs);
  if (!entry)
    return NULL;
  return nv_entry_get_name(entry);
}

static void
_fixup_sdata_handle(LogMessageSerializationState *state, NVHandle old_handle, NVHandle new_handle)
{
  LogMessage *msg = state->msg;
  gint i;

  if (msg->sdata)
    {
      for (i = 0; i < msg->num_sdata; i++)
        {
          if (state->updated_sdata_handles[i] == old_handle)
            {
              state->updated_sdata_handles[i] = new_handle;
            }
        }
    }
}

static void
_fixup_handle_in_index_entry(LogMessageSerializationState *state, NVIndexEntry *index_entry, NVHandle new_handle)
{
  NVIndexEntry *new_index_entry = &state->updated_index[state->cur_index_entry++];
  const gchar *name = _get_entry_name(state->msg->payload, index_entry);

  if (!name)
    return;

  new_index_entry->ofs = index_entry->ofs;
  new_index_entry->handle = new_handle;
  _fixup_sdata_handle(state, index_entry->handle, new_handle);
}

static inline gboolean
_is_static_entry(NVEntry *entry)
{
  return entry->name_len == 0;
}

static NVHandle
_allocate_handle_for_entry_name(NVHandle old_handle, NVEntry *entry)
{
  if (_is_static_entry(entry))
    return old_handle;
  else
    return log_msg_get_value_handle(nv_entry_get_name(entry));
}

static NVHandle
_allocate_handle_of_referenced_entry(NVTable *self, NVHandle ref_handle)
{
  NVIndexEntry *index_entry;
  NVEntry *ref_entry = nv_table_get_entry(self, ref_handle, &index_entry);

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
_fixup_entry(NVHandle handle, NVEntry *entry, NVIndexEntry *index_entry, gpointer user_data)
{
  LogMessageSerializationState *state = (LogMessageSerializationState *) user_data;
  NVTable *self = state->msg->payload;
  NVHandle new_handle;

  new_handle = _allocate_handle_for_entry_name(handle, entry);
  _fixup_handle_in_indirect_entry(self, entry);

  if (index_entry)
    _fixup_handle_in_index_entry(state, index_entry, new_handle);
              
  return FALSE;
}

void
nv_table_fixup_handles(LogMessageSerializationState *state)
{
  LogMessage *msg = state->msg;
  NVTable *nvtable = msg->payload;
  NVHandle _updated_sdata_handles[msg->num_sdata];
  NVIndexEntry _updated_index[nvtable->index_size];

  /* NOTE: we are allocating these arrays as auto variables on the stack, so
   * we can use some stack space here.  However, num_sdata is guint8,
   * index_size is guint16 */

  state->updated_sdata_handles = _updated_sdata_handles;
  state->updated_index = _updated_index;
  state->cur_index_entry = 0;

  nv_table_foreach_entry(nvtable, _fixup_entry, state);

  _copy_updated_sdata_handles(state);
  _sort_updated_index(state);
  _copy_updated_index(state);
}

/**********************************************************************
 * deserialize an NVTable
 **********************************************************************/

static gboolean
_deserialize_dyn_value(SerializeArchive *sa, NVIndexEntry* dyn_value)
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
  NVIndexEntry *index;
  index = nv_table_get_index(res);
  for (i = 0; i < res->index_size; i++)
    {
      if (!_deserialize_dyn_value(sa, &index[i]))
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
  if (!serialize_read_uint16(sa, &res->index_size))
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
_serialize_nv_dyn_value(SerializeArchive *sa, NVIndexEntry *dyn_value)
{
  serialize_write_nvhandle(sa, dyn_value->handle);
  serialize_write_uint32(sa, dyn_value->ofs);
};

static void
_write_struct(SerializeArchive *sa, NVTable *self)
{
  guint16 i;
  NVIndexEntry *index;

  serialize_write_uint32(sa, self->size);
  serialize_write_uint32(sa, self->used);
  serialize_write_uint16(sa, self->index_size);
  serialize_write_uint8(sa, self->num_static_entries);
  for (i = 0; i < self->num_static_entries; i++)
    {
      serialize_write_uint32(sa, self->static_entries[i]);
    }
  index = nv_table_get_index(self);
  for (i = 0; i < self->index_size; i++)
    {
      _serialize_nv_dyn_value(sa, &index[i]);
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
