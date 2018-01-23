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
#include "logmsg/nvtable-serialize-legacy.h"
#include "logmsg/logmsg.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>


typedef struct _NVTableMetaData
{
  guint32 magic;
  guint8 flags;
} NVTableMetaData;

/**********************************************************************
 * deserialize an NVTable
 **********************************************************************/

static gboolean
_deserialize_static_entries(SerializeArchive *sa, NVTable *res)
{
  if (!serialize_read_uint32_array(sa, res->static_entries, res->num_static_entries))
    return FALSE;
  return TRUE;
}

static gboolean
_deserialize_dynamic_entries(SerializeArchive *sa, NVTable *res)
{
  NVIndexEntry *index_table;

  index_table = nv_table_get_index(res);
  if (!serialize_read_uint32_array(sa, (guint32 *) index_table, res->index_size * 2))
    return FALSE;
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
_nv_table_deserialize_26(LogMessageSerializationState *state)
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

NVTable *
nv_table_deserialize(LogMessageSerializationState *state)
{
  if (state->version == 26)
    {
      return _nv_table_deserialize_26(state);
    }
  else if ((state->version < 26) && (state->version >= 22))
    {
      state->nvtable = nv_table_deserialize_22(state->sa);

      return state->nvtable;
    }

  g_assert_not_reached();
  return NULL;
}

/**********************************************************************
 * serialize an NVTable
 **********************************************************************/

static void
_write_struct(SerializeArchive *sa, NVTable *self)
{
  serialize_write_uint32(sa, self->size);
  serialize_write_uint32(sa, self->used);
  serialize_write_uint16(sa, self->index_size);
  serialize_write_uint8(sa, self->num_static_entries);
  serialize_write_uint32_array(sa, self->static_entries, self->num_static_entries);
  serialize_write_uint32_array(sa, (guint32 *) nv_table_get_index(self), self->index_size * 2);
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
