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
#define NVT_SUPPORTS_UNSET  0x2

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

  if (handle_a < handle_b)
    return -1;
  else if (handle_a == handle_b)
    return 0;
  else
    return 1;
}

static void
_copy_updated_sdata_handles(LogMessageSerializationState *state)
{
  memcpy(state->msg->sdata, state->updated_sdata_handles, sizeof(state->msg->sdata[0]) * state->msg->num_sdata);
}

static void
_sort_updated_index(LogMessageSerializationState *state)
{
  NVTable *self = state->nvtable;

  qsort(state->updated_index, self->index_size, sizeof(NVIndexEntry), _index_entry_cmp);
}

static void
_copy_updated_index(LogMessageSerializationState *state)
{
  NVTable *self = state->nvtable;

  memmove(nv_table_get_index(self), state->updated_index, sizeof(NVIndexEntry) * self->index_size);
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
  gint index_slot = index_entry - nv_table_get_index(state->nvtable);
  NVIndexEntry *new_index_entry = &state->updated_index[index_slot];

  new_index_entry->ofs = index_entry->ofs;
  new_index_entry->handle = new_handle;
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

static gboolean
_is_indirect(NVEntry *entry)
{
  return entry && entry->indirect;
}

static void
_fixup_handle_in_indirect_entry(NVTable *self, NVEntry *entry)
{
  entry->vindirect.handle = _allocate_handle_of_referenced_entry(self, entry->vindirect.handle);
}

static gboolean
_validate_entry(LogMessageSerializationState *state, NVEntry *entry)
{
  NVTable *nvtable = state->nvtable;

  /* check alignment */
  if ((GPOINTER_TO_UINT(entry) & 0x3) != 0)
    return FALSE;

  /* entry points above the start of the NVTable */
  if (GPOINTER_TO_UINT(entry) < GPOINTER_TO_UINT(nvtable))
    return FALSE;

  /* entry header is inside the allocated NVTable */
  if (GPOINTER_TO_UINT(entry) + NV_ENTRY_DIRECT_HDR > GPOINTER_TO_UINT(nvtable) + nvtable->size)
    return FALSE;
  /* entry as a whole is inside the allocated NVTable */
  if (GPOINTER_TO_UINT(entry) + entry->alloc_len > GPOINTER_TO_UINT(nvtable) + nvtable->size)
    return FALSE;

  if (!entry->indirect)
    {
      if (entry->alloc_len < NV_ENTRY_DIRECT_HDR + entry->name_len + 1 + entry->vdirect.value_len + 1)
        return FALSE;
    }
  else
    {
      if (entry->alloc_len < NV_ENTRY_INDIRECT_HDR + entry->name_len + 1)
        return FALSE;
    }
  return TRUE;
}

static gboolean
_update_entry(LogMessageSerializationState *state, NVEntry *entry)
{
  if ((state->nvtable_flags & NVT_SUPPORTS_UNSET) == 0)
    {
      /* if this was serialized with a syslog-ng that didn't support unset, make sure that:
       *   1) unset is set to FALSE
       *   2) the rest of the bits are cleared too
       *
       * This is needed as earlier syslog-ng versions unfortunately didn't
       * set the flags to 0, so it might contain garbage.  Anything that is
       * past NVT_SUPPORTS_UNSET however sets these bits to zero to make
       * adding new flags easier. */
      entry->unset = FALSE;
      entry->__bit_padding = 0;
    }
  return TRUE;
}

static gboolean
_fixup_entry(NVHandle handle, NVEntry *entry, NVIndexEntry *index_entry, gpointer user_data)
{
  LogMessageSerializationState *state = (LogMessageSerializationState *) user_data;
  NVTable *self = state->nvtable;
  NVHandle new_handle;

  if (!_validate_entry(state, entry) ||
      !_update_entry(state, entry))
    {
      /* this return of TRUE indicates failure, as it terminates the foreach loop */
      return TRUE;
    }

  new_handle = _allocate_handle_for_entry_name(handle, entry);

  if (_is_indirect(entry))
    _fixup_handle_in_indirect_entry(self, entry);

  if (index_entry)
    _fixup_handle_in_index_entry(state, index_entry, new_handle);
              
  if (log_msg_is_handle_sdata(new_handle))
    _fixup_sdata_handle(state, handle, new_handle);
  return FALSE;
}

gboolean
nv_table_fixup_handles(LogMessageSerializationState *state)
{
  LogMessage *msg = state->msg;
  NVTable *nvtable = state->nvtable;
  NVHandle _updated_sdata_handles[msg->num_sdata];
  NVIndexEntry _updated_index[nvtable->index_size];

  /* NOTE: we are allocating these arrays as auto variables on the stack, so
   * we can use some stack space here.  However, num_sdata is guint8,
   * index_size is guint16 */

  state->updated_sdata_handles = _updated_sdata_handles;
  state->updated_index = _updated_index;

  if (nv_table_foreach_entry(nvtable, _fixup_entry, state))
    {
      /* foreach_entry() returns TRUE if the callback returned failure */
      return FALSE;
    }

  _copy_updated_sdata_handles(state);
  _sort_updated_index(state);
  _copy_updated_index(state);
  return TRUE;
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
  NVTable *res = NULL;
  guint32 size;

  g_assert(*nvtable == NULL);

  if (!serialize_read_uint32(sa, &size))
    goto error;

  if (size > NV_TABLE_MAX_BYTES)
    goto error;

  res = (NVTable *) g_malloc(size);
  res->size = size;

  if (!serialize_read_uint32(sa, &res->used))
    goto error;

  if (!serialize_read_uint16(sa, &res->index_size))
    goto error;

  if (!serialize_read_uint8(sa, &res->num_static_entries))
    goto error;

  /* static entries has to be known by this syslog-ng, if they are over
   * LM_V_MAX, that means we have no clue how an entry is called, as static
   * entries don't contain names.  If there are less static entries, that
   * can be ok. */

  if (res->num_static_entries > LM_V_MAX)
    goto error;

  /* validates self->used and self->index_size value as compared to "size" */
  if (!nv_table_alloc_check(res, 0))
    goto error;

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
    goto error;

  if (!_read_header(sa, &res))
    goto error;

  state->nvtable_flags = meta_data.flags;
  state->nvtable = res;
  if (!_read_struct(sa, res))
    goto error;

  if (!_read_payload(sa, res))
    goto error;

  if (_has_to_swap_bytes(meta_data.flags))
    nv_table_data_swap_bytes(res);


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
  meta_data->flags |= NVT_SUPPORTS_UNSET;
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
