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

#ifndef NVTABLE_SERIALIZE_ENDIANUTILS_H_
#define NVTABLE_SERIALIZE_ENDIANUTILS_H_

static inline guint8 reverse(guint8 b)
{
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

static inline void
nv_table_swap_entry_flags(NVEntry *entry)
{
  entry->flags = reverse(entry->flags);
}

static inline void
nv_entry_swap_bytes(NVEntry *entry)
{
  nv_table_swap_entry_flags(entry);
  entry->alloc_len = GUINT32_SWAP_LE_BE(entry->alloc_len);

  if (!entry->indirect)
    {
      entry->vdirect.value_len = GUINT32_SWAP_LE_BE(entry->vdirect.value_len);
    }
  else
    {
      entry->vindirect.handle = GUINT32_SWAP_LE_BE(entry->vindirect.handle);
      entry->vindirect.ofs = GUINT32_SWAP_LE_BE(entry->vindirect.ofs);
      entry->vindirect.len = GUINT32_SWAP_LE_BE(entry->vindirect.len);
    }
}

static inline void
nv_table_dyn_value_swap_bytes(NVIndexEntry *self)
{
  self->handle = GUINT32_SWAP_LE_BE(self->handle);
  self->ofs = GUINT32_SWAP_LE_BE(self->handle);
};

static inline void
nv_table_data_swap_bytes(NVTable *self)
{
  NVIndexEntry *index_table;
  NVEntry *entry;
  gint i;

  for (i = 0; i < self->num_static_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, self->static_entries[i]);
      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }

  index_table = nv_table_get_index(self);
  for (i = 0; i < self->index_size; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, index_table[i].ofs);

      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }
}

static inline void
nv_table_struct_swap_bytes(NVTable *self)
{
  guint16 i;
  NVIndexEntry *index_table;

  self->size = GUINT16_SWAP_LE_BE(self->size);
  self->used = GUINT16_SWAP_LE_BE(self->used);
  self->index_size = GUINT16_SWAP_LE_BE(self->index_size);

  for (i = 0; i < self->num_static_entries; i++)
    {
      self->static_entries[i] = GUINT16_SWAP_LE_BE(self->static_entries[i]);
    }
  index_table = nv_table_get_index(self);

  for (i = 0; i < self->index_size; i++)
    {
      nv_table_dyn_value_swap_bytes(&index_table[i]);
    }
}

#endif /* NVTABLE_SERIALIZE_ENDIANUTILS_H_ */
