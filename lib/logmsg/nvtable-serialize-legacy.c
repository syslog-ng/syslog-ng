/*
 * Copyright (c) 2002-2012 Balabit
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

#include "nvtable-serialize-legacy.h"
#include "nvtable-serialize-endianutils.h"
#include "syslog-ng.h"
#include <string.h>

#define NV_TABLE_OLD_SCALE 2
#define NV_TABLE_MAGIC_V2  "NVT2"
static const int NV_TABLE_HEADER_DIFF_V22_V26 = 4;
static const int NV_TABLE_DYNVALUE_DIFF_V22_V26 = 4;
static const int NV_TABLE_HANDLE_DIFF_V22_V26 = 2;
static const int SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY = 12;
#define NVT_SF_BE           0x1

typedef struct _OldNVEntry
{
  /* negative offset, counting from string table top, e.g. start of
     the string is at @top + ofs */
  union
  {
    struct
    {
      guint8 indirect:1, referenced:1;
    };
    guint8 flags;
  };

  guint8 name_len;
  guint16 alloc_len;
  union
  {
    struct
    {
      guint16 value_len;
      /* variable data, first the name of this entry, then the value, both are NUL terminated */
      gchar data[0];
    } vdirect;
    struct
    {
      guint16 handle;
      guint16 ofs;
      guint16 len;
      guint8 type;
      gchar name[0];
    } vindirect;
  };
} OldNVEntry;

typedef struct _OldNVTable
{
  /* byte order indication, etc. */
  guint16 size;
  guint16 used;
  guint16 num_dyn_entries;
  guint8 num_static_entries;
  guint8 ref_cnt;

  /* variable data, see memory layout in the comment above */
  union
  {
    guint32 __dummy_for_alignment;
    guint16 static_entries[0];
    gchar data[0];
  };
} OldNVTable;

static inline void
_swap_old_entry_flags(OldNVEntry *entry)
{
  entry->flags = reverse(entry->flags);
}

static void
_old_entry_swap_bytes(OldNVEntry *entry)
{
  _swap_old_entry_flags(entry);
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

static inline int
_calculate_new_alloc_len(OldNVEntry *old_entry)
{
  return (old_entry->alloc_len << NV_TABLE_OLD_SCALE) + SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY;
}

static inline guint32
_calculate_new_size(NVTable *self)
{
  guint32 diff_of_old_used_and_new_used =
    (self->num_static_entries + self->index_size) * SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY;

  return self->size + NV_TABLE_HEADER_DIFF_V22_V26
         + NV_TABLE_HANDLE_DIFF_V22_V26 * self->num_static_entries
         + NV_TABLE_DYNVALUE_DIFF_V22_V26 * self->index_size
         + diff_of_old_used_and_new_used;
}

static inline NVEntry *
_deserialize_old_entry(GString *old_nvtable_payload, guint32 old_offset,
                       void *payload_start, gboolean different_endianness)
{
  OldNVEntry *old_entry =
    (OldNVEntry *) (old_nvtable_payload->str + old_nvtable_payload->len - old_offset);
  NVEntry *new_entry;

  if (different_endianness)
    _old_entry_swap_bytes(old_entry);

  new_entry = (NVEntry *) (payload_start - _calculate_new_alloc_len(old_entry));
  new_entry->flags = old_entry->flags;
  new_entry->name_len = old_entry->name_len;
  new_entry->alloc_len = _calculate_new_alloc_len(old_entry);

  if (!new_entry->indirect)
    {
      new_entry->vdirect.value_len = old_entry->vdirect.value_len;
      memcpy(&new_entry->vdirect.data, &old_entry->vdirect.data,
             new_entry->name_len + new_entry->vdirect.value_len + 2);
    }
  else
    {
      new_entry->vindirect.handle = old_entry->vindirect.handle;
      new_entry->vindirect.type = old_entry->vindirect.type;
      new_entry->vindirect.ofs = old_entry->vindirect.ofs;
      new_entry->vindirect.len = old_entry->vindirect.len;
      memcpy(&new_entry->vindirect.name, &old_entry->vindirect.name,
             new_entry->name_len);
    }
  return new_entry;
}

static gboolean
_deserialize_blob_v22(SerializeArchive *sa, NVTable *self, void *table_top,
                      gboolean different_endianness)
{
  GString *old_nvtable_payload;
  void *current_payload_pointer = table_top;
  int i;
  NVIndexEntry *dyn_entries;

  if (!self->used)
    return TRUE;

  old_nvtable_payload = g_string_sized_new(self->used);
  old_nvtable_payload->len = self->used;

  if (!serialize_read_blob(sa, old_nvtable_payload->str, self->used))
    {
      g_string_free(old_nvtable_payload, TRUE);
      return FALSE;
    }

  /*
   * Iterate through all NVEntries and convert them. We should update
   * their offset, too. We do not need to iterate them in order, we
   * will simply copy them to the new place linearly. The only problem
   * is that the indirect/direct use-case should be resolved
   * correctly.
   */

  for (i = 0; i < self->num_static_entries; i++)
    {
      guint32 old_entry_offset = self->static_entries[i];

      if (old_entry_offset != 0)
        {
          NVEntry *new_entry =
            _deserialize_old_entry(old_nvtable_payload, old_entry_offset,
                                   current_payload_pointer, different_endianness);

          self->static_entries[i] = (guint32)(table_top - (void *)new_entry);
          current_payload_pointer = current_payload_pointer - new_entry->alloc_len;
        }
    }

  dyn_entries = nv_table_get_index(self);
  for (i = 0; i < self->index_size; i++)
    {
      NVIndexEntry *dynvalue = &dyn_entries[i];
      guint32 old_entry_offset = dynvalue->ofs;
      NVEntry *new_entry =
        _deserialize_old_entry(old_nvtable_payload, old_entry_offset,
                               current_payload_pointer, different_endianness);

      dynvalue->ofs = (guint32) (table_top - (void *)new_entry);
      current_payload_pointer = current_payload_pointer - new_entry->alloc_len;
    }

  self->used = table_top - current_payload_pointer;

  g_string_free(old_nvtable_payload, TRUE);
  return TRUE;
}

static inline void
_convert_old_dyn_entry(NVIndexEntry *dyn_entry, guint32 old_dyn_entry)
{
  dyn_entry->handle = old_dyn_entry >> 16;
  dyn_entry->ofs = (old_dyn_entry & 0xFFFF) << NV_TABLE_OLD_SCALE;
};

static gboolean
_deserialize_struct_22(SerializeArchive *sa, NVTable *res)
{
  guint16 i;
  NVIndexEntry *dyn_entries;
  guint32 old_dyn_entry;
  guint16 old_static_entry;

  for (i = 0; i < res->num_static_entries; i++)
    {
      if (!serialize_read_uint16(sa, &old_static_entry))
        return FALSE;
      res->static_entries[i] = old_static_entry << NV_TABLE_OLD_SCALE;
    }

  dyn_entries = nv_table_get_index(res);
  for (i = 0; i < res->index_size; i++)
    {
      if (!serialize_read_uint32(sa, &old_dyn_entry))
        return FALSE;
      _convert_old_dyn_entry(&dyn_entries[i], old_dyn_entry);
    }

  return TRUE;
}

NVTable *
nv_table_deserialize_22(SerializeArchive *sa)
{
  guint16 old_res;
  guint32 magic = 0;
  guint8 flags = 0;
  NVTable *res = NULL;
  gboolean is_big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  gboolean different_endianness;

  if (!serialize_read_uint32(sa, &magic))
    return NULL;
  if (!serialize_read_uint8(sa, &flags))
    return NULL;

  if (!!(flags & NVT_SF_BE) != is_big_endian)
    {
      magic = GUINT32_SWAP_LE_BE(magic);
    }
  if (memcmp((void *) &magic, (const void *) NV_TABLE_MAGIC_V2, 4) != 0)
    return NULL;

  res = (NVTable *)g_malloc(sizeof(NVTable));

  if (!serialize_read_uint16(sa, &old_res))
    {
      g_free(res);
      return NULL;
    }
  res->size = old_res << NV_TABLE_OLD_SCALE;

  if (!serialize_read_uint16(sa, &old_res))
    {
      g_free(res);
      return NULL;
    }
  res->used = old_res << NV_TABLE_OLD_SCALE;

  if (!serialize_read_uint16(sa, &res->index_size))
    {
      g_free(res);
      return NULL;
    }

  if (!serialize_read_uint8(sa, &res->num_static_entries))
    {
      g_free(res);
      return NULL;
    }

  res->size = _calculate_new_size(res);
  res = (NVTable *)g_realloc(res, res->size);
  if(!res)
    return NULL;

  res->ref_cnt = 1;
  res->borrowed = FALSE;

  if (!_deserialize_struct_22(sa, res))
    {
      g_free(res);
      return NULL;
    }

  different_endianness = (is_big_endian != (flags & NVT_SF_BE));
  if (!_deserialize_blob_v22(sa, res, nv_table_get_top(res), different_endianness))
    {
      g_free(res);
      return NULL;
    }

  return res;
}

static inline guint32
_calculate_new_size_from_legacy_nvtable(OldNVTable *self)
{
  return self->size + NV_TABLE_HEADER_DIFF_V22_V26
         + NV_TABLE_HANDLE_DIFF_V22_V26 * self->num_static_entries
         + NV_TABLE_DYNVALUE_DIFF_V22_V26 * self->num_dyn_entries;
}

static inline guint32 *_get_legacy_dynamic_entries(OldNVTable *old)
{
  return (guint32 *)(&(old->static_entries[old->num_static_entries]));
}

static NVTable *
_create_new_nvtable_from_legacy_nvtable(OldNVTable *old)
{
  NVTable *res = g_try_malloc(_calculate_new_size_from_legacy_nvtable(old));
  NVIndexEntry *dyn_entries;
  guint32 *old_entries;
  int i;

  res->size = old->size << NV_TABLE_OLD_SCALE;
  res->used = old->used << NV_TABLE_OLD_SCALE;
  res->num_static_entries = old->num_static_entries;
  res->index_size = old->num_dyn_entries;

  for (i = 0; i < res->num_static_entries; i++)
    {
      res->static_entries[i] = old->static_entries[i] << NV_TABLE_OLD_SCALE;
    }

  old_entries = _get_legacy_dynamic_entries(old);
  dyn_entries = nv_table_get_index(res);

  for (i = 0; i < res->index_size; i++)
    {
      _convert_old_dyn_entry(&dyn_entries[i], old_entries[i]);
    }

  return res;
};

static void
_struct_swap_bytes_legacy(OldNVTable *self)
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

  dyn_entries = _get_legacy_dynamic_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      dyn_entries[i] = GUINT32_SWAP_LE_BE(dyn_entries[i]);
    }
}

NVTable *
nv_table_deserialize_legacy(SerializeArchive *sa)
{
  OldNVTable *tmp;
  NVTable *res;
  guint32 header_len = 0;
  guint32 calculated_header_len = 0;
  guint32 calculated_used_len = 0;
  guint32 used_len = 0;
  gboolean swap_bytes = FALSE;

  if (!serialize_read_uint32(sa, &header_len))
    return NULL;

  tmp = (OldNVTable *)g_try_malloc(header_len);

  if (!tmp)
    return NULL;

  if (!serialize_read_blob(sa, tmp, header_len))
    {
      g_free(tmp);
      return NULL;
    }
  calculated_header_len = sizeof(OldNVTable) +
                          tmp->num_static_entries * sizeof(tmp->static_entries[0]) +
                          tmp->num_dyn_entries * sizeof(guint32);
  if (!serialize_read_uint32(sa, &used_len))
    {
      g_free(tmp);
      return NULL;
    }

  calculated_used_len = tmp->used << NV_TABLE_OLD_SCALE;
  if (calculated_used_len != used_len || calculated_header_len != header_len)
    swap_bytes=TRUE;

  if (swap_bytes)
    _struct_swap_bytes_legacy(tmp);

  res = _create_new_nvtable_from_legacy_nvtable(tmp);
  if (!res)
    {
      g_free(tmp);
      return NULL;
    }
  g_free(tmp);

  res = (NVTable *)g_try_realloc(res, res->size);

  if (!res)
    return NULL;

  res->borrowed = FALSE;
  res->ref_cnt = 1;

  if (!_deserialize_blob_v22(sa, res, nv_table_get_top(res), swap_bytes))
    {
      g_free(res);
      return NULL;
    }

  return res;
}
