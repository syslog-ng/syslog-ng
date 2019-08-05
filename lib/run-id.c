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

gboolean
run_id_init(PersistState *state)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle;
  RunIDState *run_id_state;

  handle = persist_state_lookup_entry(state, RUN_ID_PERSIST_KEY, &size, &version);

  if (handle == 0 || size != sizeof(RunIDState))
    {
      if (handle != 0)
        msg_warning("run-id: persist state: invalid size, allocating a new one");
      handle = persist_state_alloc_entry(state, RUN_ID_PERSIST_KEY, sizeof(RunIDState));
    }

  if (!handle)
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
