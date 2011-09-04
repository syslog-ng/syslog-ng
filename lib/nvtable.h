/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef PAYLOAD_H_INCLUDED
#define PAYLOAD_H_INCLUDED

#include "syslog-ng.h"

typedef struct _NVTable NVTable;
typedef struct _NVRegistry NVRegistry;
typedef struct _NVEntry NVEntry;
typedef guint16 NVHandle;
typedef struct _NVHandleDesc NVHandleDesc;
typedef gboolean (*NVTableForeachFunc)(NVHandle handle, const gchar *name, const gchar *value, gssize value_len, gpointer user_data);
typedef gboolean (*NVTableForeachEntryFunc)(NVHandle handle, NVEntry *entry, gpointer user_data);

struct _NVHandleDesc
{
  gchar *name;
  guint16 flags;
  guint8 name_len;
};

struct _NVRegistry
{
  /* number of static names that are statically allocated in each payload */
  gint num_static_names;
  GArray *names;
  GHashTable *name_map;
};

extern const gchar *null_string;

void nv_registry_add_alias(NVRegistry *self, NVHandle handle, const gchar *alias);
NVHandle nv_registry_get_handle(NVRegistry *self, const gchar *name);
NVHandle nv_registry_alloc_handle(NVRegistry *self, const gchar *name);
void nv_registry_set_handle_flags(NVRegistry *self, NVHandle handle, guint16 flags);
NVRegistry *nv_registry_new(const gchar **static_names);
void nv_registry_free(NVRegistry *self);

static inline guint16
nv_registry_get_handle_flags(NVRegistry *self, NVHandle handle)
{
  NVHandleDesc *stored;

  if (G_UNLIKELY(!handle))
    return 0;

  stored = &g_array_index(self->names, NVHandleDesc, handle - 1);
  return stored->flags;
}

static inline const gchar *
nv_registry_get_handle_name(NVRegistry *self, NVHandle handle, gssize *length)
{
  NVHandleDesc *stored;

  if (G_UNLIKELY(!handle))
    {
      if (length)
        *length = 4;
      return "None";
    }

  stored = &g_array_index(self->names, NVHandleDesc, handle - 1);
  if (G_LIKELY(length))
    *length = stored->name_len;
  return stored->name;
}

/*
 * Contains a name-value pair.
 */
struct _NVEntry
{
  /* negative offset, counting from string table top, e.g. start of the string is at @top + ofs */
  guint8 indirect:1, referenced:1;
  guint8 name_len;
  guint16 alloc_len;
  union
  {
    struct
    {
      guint16 value_len_lo;
      guint8  value_len_hi;
      /* variable data, first the name of this entry, then the value, both are NUL terminated */
      gchar data[0];
    } vdirect;
    struct
    {
      NVHandle handle;
      guint16 ofs;
      guint16 len;
      guint8 type;
      gchar name[0];
    } vindirect;
  };
};

#define NV_ENTRY_DIRECT_HDR ((gsize) (&((NVEntry *) NULL)->vdirect.data))
#define NV_ENTRY_INDIRECT_HDR (sizeof(NVEntry))

static inline const gchar *
nv_entry_get_name(NVEntry *self)
{
  if (self->indirect)
    return self->vindirect.name;
  else
    return self->vdirect.data;
}

/*
 * Contains a set of ordered name-value pairs.
 *
 * This struct is used to track a set of name-value pairs that make up
 * a LogMessage structure. The storage layout is as concise as
 * possible to make it possible to serialize this payload as a single
 * writev() operation.
 *
 * Memory layout:
 *
 *  || struct || static value offsets || dynamic value (id, offset) pairs || <free space> || stored (name, value)  ||
 *
 * Name value area:
 *   - the name-value area grows down (e.g. lower addresses) from the end of the struct
 *   - name-value pairs are referenced by the offset counting down from the end of the struct
 *   - all NV pairs are positioned at 4 bytes boundary, thus we can index 256k with 16 bits (65536 << 2)
 *
 * Static value offsets:
 *   - a fixed size of guint16 array, containing 16 bit offsets for statically allocated entries
 *   - the handles for static values have a low value and they match the index in this array
 *
 * Dynamic values:
 *   - a dynamically sized guint32 array, the two high order bytes specify the global ID of the given value,
 *   - the low order 16 bits is the offset in this payload where
 *   - dynamic values are sorted by the global ID
 */
struct _NVTable
{
  /* byte order indication, etc. */
  guint16 size;
  guint16 used;
  guint16 num_dyn_entries;
  guint8 num_static_entries;
  guint8 ref_cnt:7,
    borrowed:1; /* specifies if the memory used by NVTable was borrowed from the container struct */

  /* variable data, see memory layout in the comment above */
  union
  {
    guint32 __dummy_for_alignment;
    guint16 static_entries[0];
    gchar data[0];
  };
};

#define NV_TABLE_SCALE 2
#define NV_TABLE_BOUND(x)  (((x) + 0x3) & ~0x3)
#define NV_TABLE_ADDR(self, x) ((gchar *) ((self)) + ((x) << NV_TABLE_SCALE))
#define NV_TABLE_ESTIMATE(value_num, string_sum)  ((value_num) * (sizeof(guint16) + sizeof(LogMessageStringTableEntry) + string_sum)
#define NV_TABLE_BOUND_NUM_STATIC(x) ((x) & ~1)


gboolean nv_table_add_value(NVTable *self, NVHandle handle, const gchar *name, gsize name_len, const gchar *value, gsize value_len, gboolean *new_entry);
gboolean nv_table_add_value_indirect(NVTable *self, NVHandle handle, const gchar *name, gsize name_len, NVHandle ref_handle, guint8 type, guint16 ofs, guint16 len, gboolean *new_entry);

gboolean nv_table_foreach(NVTable *self, NVRegistry *registry, NVTableForeachFunc func, gpointer user_data);
gboolean nv_table_foreach_entry(NVTable *self, NVTableForeachEntryFunc func, gpointer user_data);

void nv_table_clear(NVTable *self);
NVTable *nv_table_new(gint num_static_values, gint num_dyn_values, gint init_length);
NVTable *nv_table_init_borrowed(gpointer space, gsize space_len, gint num_static_entries);
NVTable *nv_table_realloc(NVTable *self);
NVTable *nv_table_clone(NVTable *self, gint additional_space);
NVTable *nv_table_ref(NVTable *self);
void nv_table_unref(NVTable *self);

static inline gsize
nv_table_get_alloc_size(gint num_static_entries, gint num_dyn_values, gint init_length)
{
  NVTable *self G_GNUC_UNUSED = NULL;
  gsize size;

  size = NV_TABLE_BOUND(init_length) + NV_TABLE_BOUND(sizeof(NVTable) + num_static_entries * sizeof(self->static_entries[0]) + num_dyn_values * sizeof(guint32));
  if (size < 256)
    return 256;
  return size;
}

static inline gchar *
nv_table_get_top(NVTable *self)
{
  return NV_TABLE_ADDR(self, self->size);
}

/* private declarations for inline functions */
NVEntry *nv_table_get_entry_slow(NVTable *self, NVHandle handle, guint32 **dyn_slot);
const gchar *nv_table_resolve_indirect(NVTable *self, NVEntry *entry, gssize *len);


static inline NVEntry *
__nv_table_get_entry(NVTable *self, NVHandle handle, guint16 num_static_entries, guint32 **dyn_slot)
{
  guint16 ofs;

  if (G_UNLIKELY(!handle))
    {
      *dyn_slot = NULL;
      return NULL;
    }

  if (G_LIKELY(handle <= num_static_entries))
    {
      ofs = self->static_entries[handle - 1];
      *dyn_slot = NULL;
      if (G_UNLIKELY(!ofs))
        return NULL;
      return (NVEntry *) (nv_table_get_top(self) - (ofs << NV_TABLE_SCALE));
    }
  else
    {
      return nv_table_get_entry_slow(self, handle, dyn_slot);
    }
}

static inline NVEntry *
nv_table_get_entry(NVTable *self, NVHandle handle, guint32 **dyn_slot)
{
  return __nv_table_get_entry(self, handle, self->num_static_entries, dyn_slot);
}

static inline const gchar *
__nv_table_get_value(NVTable *self, NVHandle handle, guint16 num_static_entries, gssize *length)
{
  NVEntry *entry;
  guint32 *dyn_slot;

  entry = nv_table_get_entry(self, handle, &dyn_slot);
  if (G_UNLIKELY(!entry))
    {
      if (length)
        *length = 0;
      return null_string;
    }

  if (!entry->indirect)
    {
      if (length)
        *length = entry->vdirect.value_len_lo + (entry->vdirect.value_len_hi << 16);
      return entry->vdirect.data + entry->name_len + 1;
    }
  return nv_table_resolve_indirect(self, entry, length);
}

static inline const gchar *
nv_table_get_value(NVTable *self, NVHandle handle, gssize *length)
{
  return __nv_table_get_value(self, handle, self->num_static_entries, length);
}


#endif
