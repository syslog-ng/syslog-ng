/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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

#include "run-id.h"
#include "persistable-state-header.h"
#include "str-format.h"
#include "messages.h"

#define RUN_ID_PERSIST_KEY "run_id"

int cached_run_id = 0;

typedef struct _RunIDState
{
  PersistableStateHeader header;
  gint run_id;
} RunIDState;


static void
_run_id_set_entry(PersistState *state, PersistEntryHandle handle, guint32 value)
{
  RunIDState *mapped_entry;

  mapped_entry = persist_state_map_entry(state, handle);
  mapped_entry->run_id = value;
  persist_state_unmap_entry(state, handle);
}

static guint32
_run_id_recover_legacy_entry(PersistState *state, PersistEntryHandle handle)
{
  guint32 old_entry;
  guint32 *mapped_entry;

  mapped_entry = persist_state_map_entry(state, handle);
  old_entry = *mapped_entry;
  persist_state_unmap_entry(state, handle);

  return old_entry;
}

static PersistEntryHandle
_run_id_process_existing_entry(PersistState *state, PersistEntryHandle handle, gsize size, guint8 version)
{
  if (size == sizeof(RunIDState))
    {
      return handle;
    }

  if (size == sizeof(guint32))
    {
      // In some legacy code we only used a single guint32 value to store the run_id. (without the proper header)
      // If the size of the entry matches exactly, we convert it. Otherwise we can not use it, and allocate a new one.
      msg_warning("run-id: persist state: found a legacy run-id state, reinitialize it");

      guint32 old_entry = _run_id_recover_legacy_entry(state, handle);
      PersistEntryHandle new_handle = persist_state_alloc_entry(state, RUN_ID_PERSIST_KEY, sizeof(RunIDState));

      if (new_handle)
        _run_id_set_entry(state, new_handle, old_entry);
      return new_handle;
    }

  msg_warning("run-id: persist state: invalid run-id found, allocating a new state",
              evt_tag_int("size", size),
              evt_tag_int("version", version));

  return persist_state_alloc_entry(state, RUN_ID_PERSIST_KEY, sizeof(RunIDState));
}

static PersistEntryHandle
_run_id_get_validated_handle(PersistState *state)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle;

  handle = persist_state_lookup_entry(state, RUN_ID_PERSIST_KEY, &size, &version);

  if (handle)
    {
      return _run_id_process_existing_entry(state, handle, size, version);
    }

  return persist_state_alloc_entry(state, RUN_ID_PERSIST_KEY, sizeof(RunIDState));
}

gboolean
run_id_init(PersistState *state)
{
  PersistEntryHandle handle;
  RunIDState *run_id_state;

  handle = _run_id_get_validated_handle(state);

  if (handle == 0)
    {
      msg_error("run-id: could not allocate persist state");
      return FALSE;
    }

  run_id_state = persist_state_map_entry(state, handle);

  run_id_state->run_id++;
  cached_run_id = run_id_state->run_id;

  persist_state_unmap_entry(state, handle);

  return TRUE;
};

int
run_id_get(void)
{
  return cached_run_id;
};

gboolean
run_id_is_same_run(gint other_id)
{
  return cached_run_id == other_id;
};

void
run_id_append_formatted_id(GString *str)
{
  if (cached_run_id)
    format_uint32_padded(str, 0, 0, 10, cached_run_id);
};

void
run_id_deinit(void)
{
  cached_run_id = 0;
};
