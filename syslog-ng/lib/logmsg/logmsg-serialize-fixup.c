/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Balázs Scheidler
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
#include "logmsg-serialize-fixup.h"
#include "nvtable-serialize.h"

#include <stdlib.h>

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
          if (msg->sdata[i] == old_handle)
            {
              state->updated_sdata_handles[i] = new_handle;
              break;
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

static gboolean
_old_handle_has_the_same_name(NVHandle old_handle, NVEntry *entry)
{
  gssize old_handle_name_len = 0;
  const gchar *old_handle_name = log_msg_get_value_name(old_handle, &old_handle_name_len);

  if (!old_handle_name)
    return FALSE;
  if (old_handle_name_len != entry->name_len)
    return FALSE;
  return memcmp(nv_entry_get_name(entry), old_handle_name, old_handle_name_len) == 0;
}

static NVHandle
_allocate_handle_for_entry_name(NVHandle old_handle, NVEntry *entry)
{
  if (_is_static_entry(entry))
    return old_handle;

  if (_old_handle_has_the_same_name(old_handle, entry))
    return old_handle;

  return log_msg_get_value_handle(nv_entry_get_name(entry));
}

static NVHandle
_allocate_handle_of_referenced_entry(NVTable *self, NVHandle ref_handle)
{
  NVEntry *ref_entry = nv_table_get_entry(self, ref_handle, NULL, NULL);

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
_fixup_entry(NVHandle old_handle, NVEntry *entry, NVIndexEntry *index_entry, gpointer user_data)
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

  new_handle = _allocate_handle_for_entry_name(old_handle, entry);

  if (index_entry)
    _fixup_handle_in_index_entry(state, index_entry, new_handle);

  if (log_msg_is_handle_sdata(new_handle))
    _fixup_sdata_handle(state, old_handle, new_handle);

  state->handle_changed = (new_handle != old_handle);

  if (_is_indirect(entry))
    _fixup_handle_in_indirect_entry(self, entry);

  return FALSE;
}

gboolean
log_msg_fixup_handles_after_deserialization(LogMessageSerializationState *state)
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
  state->handle_changed = FALSE;

  if (nv_table_foreach_entry(nvtable, _fixup_entry, state))
    {
      /* foreach_entry() returns TRUE if the callback returned failure */
      return FALSE;
    }

  if (state->handle_changed)
    {
      _copy_updated_sdata_handles(state);
      _sort_updated_index(state);
      _copy_updated_index(state);
    }
  return TRUE;
}
