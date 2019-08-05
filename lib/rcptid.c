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
#include "rcptid.h"
#include "messages.h"
#include "str-format.h"

static struct _RcptidService
{
  PersistState *persist_state;
  PersistEntryHandle persist_handle;
  GStaticMutex lock;
} rcptid_service =
{
  .lock = G_STATIC_MUTEX_INIT
};

/* NOTE: RcptIdInstance is a singleton, so we don't pass self around as an argument */

#define self (&rcpt_instance)

static RcptidState *
rcptid_map_state(void)
{
  return (RcptidState *) persist_state_map_entry(rcptid_service.persist_state, rcptid_service.persist_handle);
}

static void
rcptid_unmap_state(void)
{
  persist_state_unmap_entry(rcptid_service.persist_state, rcptid_service.persist_handle);
}

static gboolean
rcptid_is_initialized(void)
{
  return rcptid_service.persist_state != NULL;
}

static gboolean
rcptid_restore_entry(void)
{
  RcptidState *data = rcptid_map_state();

  if (data->header.version > 0)
    {
      msg_error("Internal error restoring log reader state, stored data is too new",
                evt_tag_int("version", data->header.version));
      return FALSE;
    }

  if ((data->header.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
      (!data->header.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
    {
      data->header.big_endian = !data->header.big_endian;
      data->g_rcptid = GUINT64_SWAP_LE_BE(data->g_rcptid);
    }

  rcptid_unmap_state();
  return TRUE;
}

static gboolean
rcptid_create_new_entry(void)
{
  RcptidState *data;

  rcptid_service.persist_handle = persist_state_alloc_entry(rcptid_service.persist_state, "next.rcptid",
                                                            sizeof(RcptidState));
  if (!rcptid_service.persist_handle)
    {
      msg_error("Error allocating RCPTID structure in persist-state");
      return FALSE;
    }

  data = rcptid_map_state();
  data->header.version = 0;
  data->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  data->g_rcptid = 1;
  rcptid_unmap_state();
  return TRUE;
}

void
rcptid_append_formatted_id(GString *result, guint64 rcptid)
{
  if (rcptid)
    {
      format_uint64_padded(result, 0, 0, 10, rcptid);
    }
}

void
rcptid_set_id(guint64 id)
{
  RcptidState *data;

  if (!rcptid_is_initialized())
    return;

  g_static_mutex_lock(&rcptid_service.lock);

  data = rcptid_map_state();
  data->g_rcptid = id;

  rcptid_unmap_state();

  g_static_mutex_unlock(&rcptid_service.lock);
}

guint64
rcptid_generate_id(void)
{
  RcptidState *data;
  guint64 new_id;

  if (!rcptid_is_initialized())
    return 0;

  g_static_mutex_lock(&rcptid_service.lock);

  data = rcptid_map_state();

  new_id = data->g_rcptid++;
  if (data->g_rcptid == 0)
    data->g_rcptid = 1;

  rcptid_unmap_state();

  g_static_mutex_unlock(&rcptid_service.lock);

  return new_id;
}

/*restore RCTPID from persist file, if possible, else
  create new enrty point with "next.rcptid" name*/
gboolean
rcptid_init(PersistState *state, gboolean use_rcptid)
{
  gsize size;
  guint8 version;

  g_assert(rcptid_service.persist_state == NULL);
  if (!use_rcptid)
    return TRUE;

  rcptid_service.persist_state = state;
  rcptid_service.persist_handle = persist_state_lookup_entry(state, "next.rcptid", &size, &version);

  if (rcptid_service.persist_handle && size == sizeof(RcptidState))
    {
      return rcptid_restore_entry();
    }
  else
    {
      if (rcptid_service.persist_handle)
        msg_warning("rcpt-id: persist state: invalid size, allocating a new one");
      return rcptid_create_new_entry();
    }
}

void
rcptid_deinit(void)
{
  rcptid_service.persist_state = NULL;
}
