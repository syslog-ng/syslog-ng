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
  guint64 committed;            // last fully confirmed offset
  guint64 window_start;         // first bit in bitmap corresponds to this offset
  guint64 window_size;          // number of bits allocated
  gboolean end_offset_resolved; // whether RD_KAFKA_OFFSET_END has been resolved to a concrete offset
  guint8 *bitmap;               // the bit array

  KafkaSourcePersist *owner;    // This is just for easier debugging
} OffsetTracker;

typedef struct _KafkaSourcePersistData
{
  PersistableStateHeader header;
  int64_t position;
  gboolean use_offset_tracker;
  KafkaSrcPersistStore persist_store;
} KafkaSourcePersistData;

struct _KafkaSourcePersist
{
  PersistState *persist_state;
  PersistEntryHandle handle;

  gboolean use_offset_tracker;
  OffsetTracker *tracker;

  KafkaSourceDriver *owner;
  gchar topic[MAX_KAFKA_TOPIC_NAME_LEN + 1];
  int32_t partition;
  int64_t last_confirmed_offset;

  GMutex mutex;
  gboolean valid;
  guint used_count;
};

typedef struct _KafkaSourceBookmark
{
  PersistEntryHandle persist_handle;
  KafkaSourcePersistData data;
  KafkaSourcePersist *persist;
} KafkaSourceBookmark;


OffsetTracker *
offset_tracker_new(KafkaSourcePersist *owner, gint64 committed_offset)
{
  OffsetTracker *t = g_new0(OffsetTracker, 1);

  t->owner = owner;

  /* normalize, if no previous offset (committed_offset < 0) than start from -1 (means next valid is 0)
   *
   * RD_KAFKA_OFFSET_BEGINNING - first offset is know, 0, so we can set up the tracker right away
   *
   * RD_KAFKA_OFFSET_STORED
   * RD_KAFKA_OFFSET_END       - means we don't know the end-offset here yet
   *                             (librdkafka either cannot provide it if there was no committed offset yet, or we use remote store),
   *                             so we have to handle it at the first message consumption time in offset_tracker_bitmap_set.
   *                             It is needed to eliminate a huge gap if there are no committed offsets yet and the topic has a high offset.
   */
  t->end_offset_resolved = (committed_offset == RD_KAFKA_OFFSET_BEGINNING || committed_offset >= 0);

  if (committed_offset < 0)
    committed_offset = -1;

  t->committed = committed_offset;
  t->window_start = t->committed + 1;  // next expected offset
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
    if (t->bitmap[i] != 0)
      {
        has_uncommitted = TRUE;
        break;
      }

  /* This is acceptable if not all messages are processed yet and e.g. we exit */
  if (has_uncommitted)
    kafka_msg_debug("kafka: offset_tracker being freed with uncommitted offsets",
                    evt_tag_long("window_start", t->window_start),
                    evt_tag_long("committed", t->committed),
                    evt_tag_int("window_size", t->window_size),
                    evt_tag_str("topic", t->owner->topic),
                    evt_tag_int("partition", (int) t->owner->partition));

  g_free(t->bitmap);
  g_free(t);
}

static inline gboolean
offset_tracker_is_ready(OffsetTracker *t)
{
  return t->end_offset_resolved;
}

static inline void
offset_tracker_bitmap_set(OffsetTracker *t, guint64 offset)
{
  /* Handle [RD_KAFKA_OFFSET_END, RD_KAFKA_OFFSET_STORED+remote] resolution on the first message, see offset_tracker_new for details */
  if (G_UNLIKELY(FALSE == offset_tracker_is_ready(t)))
    {
      t->end_offset_resolved = TRUE;

      t->committed = offset - 1;
      t->window_start = t->committed + 1;
    }

  /* Important sanity check, if offset is behind window_start, something is wrong, see KafkaSourceWorker::_run_consumer for details */
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
                          evt_tag_long("gap", idx - t->window_size),
                          evt_tag_str("topic", t->owner->topic),
                          evt_tag_int("partition", (int) t->owner->partition));
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
                          evt_tag_long("committed", t->committed),
                          evt_tag_str("topic", t->owner->topic),
                          evt_tag_int("partition", (int) t->owner->partition));
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
              //                 evt_tag_int("window_size", t->window_size),
              //                 evt_tag_str("persist_name", t->persist_name));

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

static void
_save_bookmark(Bookmark *bookmark)
{
  KafkaSourceBookmark *kafka_source_bookmark = (KafkaSourceBookmark *)(&bookmark->container);
  KafkaSourcePersist *persist = kafka_source_bookmark->persist;
  KafkaSourcePersistData *persistable_data = &kafka_source_bookmark->data;
  int64_t position_to_store = persistable_data->position;
  gboolean should_store = TRUE;

  if (persistable_data->use_offset_tracker)
    {
      OffsetTracker *tracker = persist->tracker;

      offset_tracker_bitmap_set(tracker, position_to_store);
      /* Persist only if committed advanced */
      if ((should_store = offset_tracker_try_bitmap_shift(tracker)))
        position_to_store = tracker->committed;
    }

  if (should_store)
    {
      gboolean stored = TRUE;
      if (persistable_data->persist_store == KSPS_REMOTE)
        {
          g_mutex_lock(&persist->mutex);
          /* If the persist is invalidated it means there is no Kafka connection anymore to store the offset */
          if (persist->valid)
            kafka_store_offset(persist->owner, persist->topic, persist->partition, position_to_store);
          else
            {
              kafka_msg_debug("kafka: persist is invalidated, cannot store offset remotely",
                              evt_tag_str("topic", persist->topic),
                              evt_tag_int("partition", (int) persist->partition),
                              evt_tag_long("offset", position_to_store));
              stored = FALSE;
            }
          g_mutex_unlock(&persist->mutex);
        }
      else
        {
          KafkaSourcePersistData *persist_entry = persist_state_map_entry(bookmark->persist_state,
                                                  kafka_source_bookmark->persist_handle);
          persist_entry->position = position_to_store;
          persist_state_unmap_entry(bookmark->persist_state, kafka_source_bookmark->persist_handle);
        }
      if (stored)
        {
          /* Lazy update is acceptable as the reference is kept till the function returns,
           * and only one _save_bookmark can be active at a time for a given persist */
          persist->last_confirmed_offset = position_to_store;
          msg_trace("kafka: message got confirmed",
                    evt_tag_str("topic", persist->topic),
                    evt_tag_int("partition", (int) persist->partition),
                    evt_tag_long("offset", position_to_store));
        }
    }
  kafka_source_persist_release(persist);
}

void
kafka_source_persist_fill_bookmark(KafkaSourcePersist *self, Bookmark *bookmark, int64_t offset)
{
  g_mutex_lock(&self->mutex);

  KafkaSourceBookmark *kafka_source_bookmark = (KafkaSourceBookmark *)(&bookmark->container);
  KafkaSourcePersistData *persistable_data = &kafka_source_bookmark->data;

  self->used_count++;
  kafka_source_bookmark->persist = self;
  kafka_source_bookmark->persist_handle = self->handle;

  persistable_data->position = offset;
  persistable_data->use_offset_tracker = self->use_offset_tracker;
  persistable_data->persist_store = self->owner->options.persist_store;

  bookmark->save = _save_bookmark;

  g_mutex_unlock(&self->mutex);
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

static void
_persist_data_defaults(KafkaSourcePersistData *persist_data)
{
  memset(persist_data, 0, sizeof(KafkaSourcePersistData));

  persist_data->header.version = KAFKA_SOURCE_PERSIST_VERSION_1;
  persist_data->header.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  persist_data->position = RD_KAFKA_OFFSET_INVALID;
}

static gboolean
_load_persist_entry(KafkaSourcePersist *self, const gchar *persist_name, int64_t override_position)
{
  PersistEntryHandle persist_handle = _lookup_handle(self, persist_name);

  if (!persist_handle)
    return FALSE;

  g_assert(_check_compatibility(self, persist_handle, persist_name));

  self->handle = persist_handle;
  if (self->use_offset_tracker)
    {
      KafkaSourcePersistData *persist_data = persist_state_map_entry(self->persist_state, persist_handle);

      /* RD_KAFKA_OFFSET_STORED - Means no override, use saved offsets if available, but only if persist_store is local */
      gboolean use_locally_saved_offset = (override_position == RD_KAFKA_OFFSET_STORED &&
                                           self->owner->options.persist_store == KSPS_LOCAL);
      self->tracker = offset_tracker_new(self,
                                         use_locally_saved_offset ?
                                         persist_data->position :
                                         override_position);
      persist_state_unmap_entry(self->persist_state, persist_handle);
    }

  return TRUE;
}

static gboolean
_create_persist_entry(KafkaSourcePersist *self, const gchar *persist_name, int64_t override_position)
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
    self->tracker = offset_tracker_new(self, override_position);
  persist_state_unmap_entry(self->persist_state, persist_handle);

  self->handle = persist_handle;

  msg_trace("kafka: kafka-source persist entry created", evt_tag_str("persist_name", persist_name));

  return TRUE;
}

gboolean
kafka_source_persist_init(KafkaSourcePersist *self,
                          PersistState *persist_state,
                          const gchar *topic,
                          int32_t partition,
                          int64_t override_position,
                          gboolean use_offset_tracker)
{
  self->use_offset_tracker = use_offset_tracker;
  self->persist_state = persist_state;
  g_strlcpy(self->topic, topic, sizeof(self->topic));
  self->partition = partition;

  gchar persist_name[MAX_KAFKA_PARTITION_KEY_NAME_LEN];
  kafka_format_partition_key(topic, partition, persist_name, sizeof(persist_name));
  return _load_persist_entry(self, persist_name, override_position) ||
         _create_persist_entry(self, persist_name, override_position);
}

void kafka_source_persist_invalidate(KafkaSourcePersist *self)
{
  g_mutex_lock(&self->mutex);
  self->valid = FALSE;
  g_mutex_unlock(&self->mutex);
}

inline gboolean
kafka_source_persist_is_ready(KafkaSourcePersist *self)
{
  g_mutex_lock(&self->mutex);
  gboolean ready = self->use_offset_tracker ? offset_tracker_is_ready(self->tracker) : TRUE;
  g_mutex_unlock(&self->mutex);
  return ready;
}

inline gboolean
kafka_source_persist_matching(KafkaSourcePersist *self,
                              const gchar *topic,
                              int32_t partition)
{
  return (g_strcmp0(self->topic, topic) == 0) && (self->partition == partition);
}

KafkaSourcePersist *
kafka_source_persist_new(KafkaSourceDriver *owner)
{
  KafkaSourcePersist *self = g_new0(KafkaSourcePersist, 1);
  g_mutex_init(&self->mutex);
  self->owner = owner;
  self->used_count = 1;
  self->last_confirmed_offset = RD_KAFKA_OFFSET_INVALID;
  self->valid = TRUE;
  return self;
}

static void
_kafka_source_persist_free(KafkaSourcePersist *self)
{
  kafka_msg_debug("kafka: last confirmed offset stored for persist",
                  evt_tag_str("topic", self->topic),
                  evt_tag_int("partition", (int) self->partition),
                  evt_tag_long("offset", self->last_confirmed_offset));
  self->valid = FALSE;

  if (self->tracker)
    offset_tracker_free(self->tracker);

  g_mutex_unlock(&self->mutex);
  g_mutex_clear(&self->mutex);

  memset(self, 0, sizeof(KafkaSourcePersist));
  g_free(self);
}

void kafka_source_persist_release(KafkaSourcePersist *self)
{
  g_mutex_lock(&self->mutex);
  self->used_count--;
  if (self->used_count == 0)
    _kafka_source_persist_free(self);
  else
    g_mutex_unlock(&self->mutex);
}
