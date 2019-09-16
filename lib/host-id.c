/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Laszlo Budai
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

#include "host-id.h"
#include "str-format.h"
#include "messages.h"
#include <openssl/rand.h>

guint32 global_host_id = 0;

static guint32
_create_host_id(void)
{
  union
  {
    unsigned char _raw[sizeof(guint32)];
    guint32 id;
  } host_id;

  RAND_bytes(host_id._raw, sizeof(host_id._raw));

  return host_id.id;
}

static gboolean
_load_host_id_from_legacy_persist_entry(PersistState *state, guint32 *host_id)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle = persist_state_lookup_entry(state, HOST_ID_LEGACY_PERSIST_KEY, &size, &version);

  if (!handle)
    return FALSE;

  guint32 *mapped_hostid = persist_state_map_entry(state, handle);

  *host_id = *mapped_hostid;

  persist_state_unmap_entry(state, handle);

  return TRUE;
}

gboolean
host_id_init(PersistState *state)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle;
  HostIdState *host_id_state;
  gboolean host_id_found = TRUE;
  guint32 legacy_hostid = 0;


  handle = persist_state_lookup_entry(state, HOST_ID_PERSIST_KEY, &size, &version);
  if (handle == 0)
    {
      host_id_found = FALSE;

      handle = persist_state_alloc_entry(state, HOST_ID_PERSIST_KEY, sizeof(HostIdState));
      if (handle == 0)
        {
          msg_error("host-id: could not allocate persist state");
          return FALSE;
        }
    }

  host_id_state = persist_state_map_entry(state, handle);

  if (!host_id_found)
    {
      if (_load_host_id_from_legacy_persist_entry(state, &legacy_hostid))
        {
          host_id_state->host_id = legacy_hostid;
        }
      else
        {
          host_id_state->host_id = _create_host_id();
        }
    }

  global_host_id = host_id_state->host_id;

  persist_state_unmap_entry(state, handle);

  return TRUE;
}

guint32
host_id_get(void)
{
  return global_host_id;
}

void
host_id_append_formatted_id(GString *str, guint32 id)
{
  format_uint32_padded(str, 8, '0', 16, id);
}
