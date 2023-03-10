/*
 * Copyright (c) 2020 Attila Szakacs <attila.szakacs@balabit.com>
 * Copyright (c) 2023 Hofi <hofione@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "darwinosl-source-persist.h"
#include "messages.h"
#include "persistable-state-header.h"
#include "ack-tracker/ack_tracker.h"

#define DARWINOSL_SOURCE_PERSIST_VERSION_1 1

typedef struct _DarwinOSLogSourcePersistData
{
  PersistableStateHeader header;
  DarwinOSLogSourcePosition position;
} DarwinOSLogSourcePersistData;

typedef struct _DarwinOSLogSourceBookmark
{
  PersistEntryHandle persist_handle;
  DarwinOSLogSourcePersistData data;
} DarwinOSLogSourceBookmark;

struct _DarwinOSLogSourcePersist
{
  PersistState *persist_state;
  PersistEntryHandle handle;
};

DarwinOSLogSourcePersist *
darwinosl_source_persist_new(void)
{
  DarwinOSLogSourcePersist *self = g_new0(DarwinOSLogSourcePersist, 1);
  return self;
}

void
darwinosl_source_persist_free(DarwinOSLogSourcePersist *self)
{
  g_free(self);
}

static PersistEntryHandle
_lookup_handle(DarwinOSLogSourcePersist *self, const gchar *persist_name)
{
  gsize size;
  guint8 version;
  return persist_state_lookup_entry(self->persist_state, persist_name, &size, &version);
}

static gboolean
_check_version(DarwinOSLogSourcePersistData *persist_data, const gchar *persist_name)
{
  switch (persist_data->header.version)
    {
    case DARWINOSL_SOURCE_PERSIST_VERSION_1:
      return TRUE;
    /* Conversion functions might come here. */
    default:
      msg_error("Incompatible darwinosl-source persist version in persist file",
                evt_tag_str("persist_name", persist_name),
                evt_tag_int("version", persist_data->header.version),
                evt_tag_int("expected_version", DARWINOSL_SOURCE_PERSIST_VERSION_1));
      return FALSE;
    }
}

static gboolean
_check_compatibility(DarwinOSLogSourcePersist *self, PersistEntryHandle persist_handle, const gchar *persist_name)
{
  DarwinOSLogSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);
  g_assert(persist_data);

  if (!_check_version(persist_data, persist_name))
    {
      persist_state_unmap_entry(self->persist_state, persist_handle);
      return FALSE;
    }

  persist_state_unmap_entry(self->persist_state, persist_handle);
  return TRUE;
}

static gboolean
_load_persist_entry(DarwinOSLogSourcePersist *self, const gchar *persist_name)
{
  PersistEntryHandle persist_handle = _lookup_handle(self, persist_name);

  if (!persist_handle)
    return FALSE;

  g_assert(_check_compatibility(self, persist_handle, persist_name));

  self->handle = persist_handle;

  return TRUE;
}

static void
_persist_data_defaults(DarwinOSLogSourcePersistData *persist_data)
{
  memset(persist_data, 0, sizeof(DarwinOSLogSourcePersistData));

  persist_data->header.version = DARWINOSL_SOURCE_PERSIST_VERSION_1;
  persist_data->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
}

static gboolean
_create_persist_entry(DarwinOSLogSourcePersist *self, const gchar *persist_name)
{
  PersistEntryHandle persist_handle = persist_state_alloc_entry(self->persist_state, persist_name,
                                      sizeof(DarwinOSLogSourcePersistData));
  if (!persist_handle)
    {
      msg_error("Could not allocate darwinosl-source persist entry",
                evt_tag_str("persist_name", persist_name));
      return FALSE;
    }

  DarwinOSLogSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);
  if (!persist_data)
    {
      msg_error("Could not map darwinosl-source persist state",
                evt_tag_str("persist_name", persist_name));
      return FALSE;
    }

  _persist_data_defaults(persist_data);

  persist_state_unmap_entry(self->persist_state, persist_handle);

  self->handle = persist_handle;

  msg_trace("darwinosl-source persist entry created",
            evt_tag_str("persist_name", persist_name));

  return TRUE;
}

gboolean
darwinosl_source_persist_init(DarwinOSLogSourcePersist *self, PersistState *persist_state, const gchar *persist_name)
{
  self->persist_state = persist_state;

  return _load_persist_entry(self, persist_name) ||
         _create_persist_entry(self, persist_name);
}

static void
_save_bookmark(Bookmark *bookmark)
{
  DarwinOSLogSourceBookmark *darwinosl_source_bookmark = (DarwinOSLogSourceBookmark *)(&bookmark->container);
  DarwinOSLogSourcePersistData *persistable_data = &darwinosl_source_bookmark->data;
  DarwinOSLogSourcePersistData *persist_entry = persist_state_map_entry(bookmark->persist_state,
                                                darwinosl_source_bookmark->persist_handle);

  persist_entry->position = persistable_data->position;

  persist_state_unmap_entry(bookmark->persist_state, darwinosl_source_bookmark->persist_handle);
}

void
darwinosl_source_persist_fill_bookmark(DarwinOSLogSourcePersist *self, Bookmark *bookmark,
                                       DarwinOSLogSourcePosition position)
{
  DarwinOSLogSourceBookmark *darwinosl_source_bookmark = (DarwinOSLogSourceBookmark *)(&bookmark->container);
  DarwinOSLogSourcePersistData *persistable_data = &darwinosl_source_bookmark->data;

  darwinosl_source_bookmark->persist_handle = self->handle;
  persistable_data->position = position;

  bookmark->save = _save_bookmark;
}

void
darwinosl_source_persist_load_position(DarwinOSLogSourcePersist *self, DarwinOSLogSourcePosition *position)
{
  DarwinOSLogSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, self->handle);
  g_assert(persist_data);

  *position = persist_data->position;

  persist_state_unmap_entry(self->persist_state, self->handle);
}
