/*
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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

#include "kafka-source-persist.h"
#include "kafka-internal.h"
#include "messages.h"
#include "persistable-state-header.h"
#include "ack-tracker/ack_tracker.h"

#define KAFKA_SOURCE_PERSIST_VERSION_1 1
#define MIN_WINDOW_SIZE 2048

typedef struct _OffsetTracker
{
  guint64 committed;      // last fully confirmed offset
  guint64 window_start;   // first bit in bitmap corresponds to this offset
  guint64 window_size;    // number of bits allocated
  guint8 *bitmap;         // the bit array
} OffsetTracker;

typedef struct _KafkaSourcePersistData
{
  PersistableStateHeader header;
  int64_t position;
} KafkaSourcePersistData;

struct _KafkaSourcePersist
{
  PersistState *persist_state;
  PersistEntryHandle handle;
  gboolean use_offset_tracker;
  OffsetTracker *tracker;
};

typedef struct _KafkaSourceBookmark
{
  PersistEntryHandle persist_handle;
  KafkaSourcePersistData data;
  KafkaSourcePersist *persist;
} KafkaSourceBookmark;


OffsetTracker *
offset_tracker_new(gint64 committed_offset)
{
  OffsetTracker *t = g_new0(OffsetTracker, 1);

  /* normalize, if no previous offset, start from -1 (means next valid is 0) */
  if (committed_offset < 0)
    committed_offset = -1;

  t->committed = committed_offset;
  t->window_start = committed_offset + 1;  // next expected offset
  t->window_size = MIN_WINDOW_SIZE;

  t->bitmap = g_malloc0((t->window_size + 7) / 8);

  return t;
}

void
offset_tracker_free(OffsetTracker *t)
{
  /* Sanity check: verify all offsets in the window have been committed */
  guint64 total_bytes = (t->window_size + 7) / 8;
  gboolean has_uncommitted = FALSE;

  for (guint64 i = 0; i < total_bytes; i++)
    {
      if (t->bitmap[i] != 0)
        {
          has_uncommitted = TRUE;
          break;
        }
    }

  if (has_uncommitted)
    kafka_msg_debug("kafka: offset_tracker being freed with uncommitted offsets",
                    evt_tag_long("window_start", t->window_start),
                    evt_tag_long("committed", t->committed),
                    evt_tag_int("window_size", t->window_size));

  g_free(t->bitmap);
  g_free(t);
}

static inline void
offset_tracker_bitmap_set(OffsetTracker *t, guint64 offset)
{
  /* Sanity check: if offset is way behind window_start, something is wrong */
  g_assert(offset >= t->window_start);
  guint64 idx = offset - t->window_start;

  if (idx >= t->window_size)
    {
      /* Check if the gap is suspiciously large - might indicate a problem */
      if (idx > t->window_size * 4)
        {
          kafka_msg_debug("kafka: offset_tracker detected large gap, possible out-of-order or offset jump",
                          evt_tag_long("offset", offset),
                          evt_tag_long("window_start", t->window_start),
                          evt_tag_long("committed", t->committed),
                          evt_tag_int("current_window_size", t->window_size),
                          evt_tag_long("gap", idx - t->window_size));
        }

      guint64 new_size = MAX(idx + 1, t->window_size * 2);
      guint64 old_bytes = (t->window_size + 7) / 8;
      guint64 new_bytes = (new_size + 7) / 8;

      if (new_bytes > old_bytes)
        {
          t->bitmap = g_realloc(t->bitmap, new_bytes);
          memset(t->bitmap + old_bytes, 0, new_bytes - old_bytes);
          kafka_msg_debug("kafka: offset_tracker expanded window",
                          evt_tag_int("old_size", t->window_size),
                          evt_tag_int("new_size", new_size),
                          evt_tag_long("offset", offset),
                          evt_tag_long("window_start", t->window_start),
                          evt_tag_long("committed", t->committed));
        }
      t->window_size = new_size;
    }

  t->bitmap[idx / 8] |= (1 << (idx % 8));
}

static inline gboolean
offset_tracker_try_bitmap_shift(OffsetTracker *t)
{
  guint64 committed_before = t->committed;

  while (TRUE)
    {
      guint64 next = t->committed + 1;
      guint64 idx = next - t->window_start;
      g_assert(idx < t->window_size);
      const guint64 idx_byte = idx / 8;

      /* exit at the first not set bit */
      if ((t->bitmap[idx_byte] & (1 << (idx % 8))) == 0)
        break;

      t->committed++;

      /* clear consumed bit */
      t->bitmap[idx_byte] &= ~(1 << (idx % 8));
    }

  /* After advancing committed, check if we should shift the window */
  if (t->committed > committed_before)
    {
      guint64 distance_from_start = t->committed - t->window_start;

      /* Shift if committed is > MIN_WINDOW_SIZE / 2 bits ahead of window_start */
      if (distance_from_start > MIN_WINDOW_SIZE / 2)
        {
          /* Shift to align window_start with committed */
          guint64 shift_amount = distance_from_start;
          guint64 shift_bytes = (shift_amount / 8) & ~7;  // round down to multiple of 64 bits for alignment

          if (shift_bytes > 0)
            {
              const guint64 total_bytes = (t->window_size + 7) / 8;

              memcpy(t->bitmap, t->bitmap + shift_bytes, total_bytes - shift_bytes);
              memset(t->bitmap + total_bytes - shift_bytes, 0, shift_bytes);

              t->window_start += shift_bytes * 8;

              // kafka_msg_trace("kafka: offset_tracker shifted window",
              //                 evt_tag_int("shift_bits", shift_bytes * 8),
              //                 evt_tag_long("new_window_start", t->window_start),
              //                 evt_tag_long("committed", t->committed),
              //                 evt_tag_int("window_size", t->window_size));

              /* TODO: add shrinking */
            }
        }
      return TRUE;
    }
  return FALSE;
}

static PersistEntryHandle
_lookup_handle(KafkaSourcePersist *self, const gchar *persist_name)
{
  gsize size;
  guint8 version;
  return persist_state_lookup_entry(self->persist_state, persist_name, &size, &version);
}

static gboolean
_check_version(KafkaSourcePersistData *persist_data, const gchar *persist_name)
{
  switch (persist_data->header.version)
    {
    case KAFKA_SOURCE_PERSIST_VERSION_1:
      return TRUE;
    /* Auto-conversion functions might come here. */
    default:
      msg_error("kafka: incompatible kafka-source persist version in persist file",
                evt_tag_str("persist_name", persist_name),
                evt_tag_int("version", persist_data->header.version),
                evt_tag_int("expected_version", KAFKA_SOURCE_PERSIST_VERSION_1));
      return FALSE;
    }
}

static gboolean
_check_compatibility(KafkaSourcePersist *self, PersistEntryHandle persist_handle, const gchar *persist_name)
{
  KafkaSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);
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
_load_persist_entry(KafkaSourcePersist *self, const gchar *persist_name)
{
  PersistEntryHandle persist_handle = _lookup_handle(self, persist_name);

  if (!persist_handle)
    return FALSE;

  g_assert(_check_compatibility(self, persist_handle, persist_name));

  self->handle = persist_handle;
  if (self->use_offset_tracker)
    {
      KafkaSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);
      self->tracker = offset_tracker_new(persist_data->position);
      persist_state_unmap_entry(self->persist_state, persist_handle);
    }

  return TRUE;
}

static void
_persist_data_defaults(KafkaSourcePersistData *persist_data)
{
  memset(persist_data, 0, sizeof(KafkaSourcePersistData));

  persist_data->header.version = KAFKA_SOURCE_PERSIST_VERSION_1;
  persist_data->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  persist_data->position = RD_KAFKA_OFFSET_INVALID;
}

static gboolean
_create_persist_entry(KafkaSourcePersist *self, const gchar *persist_name)
{
  PersistEntryHandle persist_handle = persist_state_alloc_entry(self->persist_state, persist_name,
                                      sizeof(KafkaSourcePersistData));
  if (!persist_handle)
    {
      msg_error("kafka: could not allocate kafka-source persist entry", evt_tag_str("persist_name", persist_name));
      return FALSE;
    }

  KafkaSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);
  if (!persist_data)
    {
      msg_error("kafka: could not map kafka-source persist state", evt_tag_str("persist_name", persist_name));
      return FALSE;
    }

  _persist_data_defaults(persist_data);
  if (self->use_offset_tracker)
    self->tracker = offset_tracker_new(RD_KAFKA_OFFSET_INVALID);

  persist_state_unmap_entry(self->persist_state, persist_handle);

  self->handle = persist_handle;

  msg_trace("kafka: kafka-source persist entry created", evt_tag_str("persist_name", persist_name));

  return TRUE;
}

static void
_save_bookmark(Bookmark *bookmark)
{
  KafkaSourceBookmark *kafka_source_bookmark = (KafkaSourceBookmark *)(&bookmark->container);
  KafkaSourcePersist *persist = kafka_source_bookmark->persist;
  KafkaSourcePersistData *persistable_data = &kafka_source_bookmark->data;

  KafkaSourcePersistData *persist_entry = persist_state_map_entry(bookmark->persist_state,
                                          kafka_source_bookmark->persist_handle);
  if (persist->use_offset_tracker)
    {
      /* TODO: Intentionally not using guards/locks currently, check if bookmark saving
       *       can be called concurrently, as multiple threads can process the same topic/partition
       *       in which case only a single persist is allocated for the given topic/partition combination */
      OffsetTracker *tracker = persist->tracker;

      offset_tracker_bitmap_set(tracker, persistable_data->position);
      /* Persist only if committed advanced */
      if (offset_tracker_try_bitmap_shift(tracker))
        {
          persist_entry->position = persistable_data->position;
          // kafka_msg_trace("kafka: message got confirmed", evt_tag_long("offset", persistable_data->position));
        }
    }
  else
    {
      persist_entry->position = persistable_data->position;
      // kafka_msg_trace("kafka: message got confirmed", evt_tag_long("offset", persistable_data->position));
    }

  persist_state_unmap_entry(bookmark->persist_state, kafka_source_bookmark->persist_handle);
}

void
kafka_source_persist_fill_bookmark(KafkaSourcePersist *self, Bookmark *bookmark, int64_t offset)
{
  KafkaSourceBookmark *kafka_source_bookmark = (KafkaSourceBookmark *)(&bookmark->container);
  KafkaSourcePersistData *persistable_data = &kafka_source_bookmark->data;

  kafka_source_bookmark->persist_handle = self->handle;
  kafka_source_bookmark->persist = self;
  persistable_data->position = offset;

  bookmark->save = _save_bookmark;
}

void
kafka_source_persist_load_position(KafkaSourcePersist *self,
                                   int64_t *offset)
{
  KafkaSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, self->handle);
  g_assert(persist_data);

  *offset = persist_data->position;

  persist_state_unmap_entry(self->persist_state, self->handle);
}

gboolean
kafka_source_persist_init(KafkaSourcePersist *self, PersistState *persist_state, const gchar *persist_name)
{
  self->persist_state = persist_state;

  return _load_persist_entry(self, persist_name) ||
         _create_persist_entry(self, persist_name);
}

KafkaSourcePersist *
kafka_source_persist_new(gboolean use_offset_tracker)
{
  KafkaSourcePersist *self = g_new0(KafkaSourcePersist, 1);
  self->use_offset_tracker = use_offset_tracker;
  return self;
}

void
kafka_source_persist_free(KafkaSourcePersist *self)
{
  if (self->tracker)
    offset_tracker_free(self->tracker);
  g_free(self);
}
