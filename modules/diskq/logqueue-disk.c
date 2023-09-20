/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "logqueue-disk.h"
#include "logpipe.h"
#include "messages.h"
#include "serialize.h"
#include "logmsg/logmsg-serialize.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "reloc.h"
#include "qdisk.h"
#include "scratch-buffers.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define B_TO_KiB(x) ((x) / 1024)

QueueType log_queue_disk_type = "DISK";

gboolean
log_queue_disk_stop(LogQueue *s, gboolean *persistent)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  g_assert(self->stop);

  if (!qdisk_started(self->qdisk))
    {
      *persistent = FALSE;
      return TRUE;
    }

  log_queue_queued_messages_sub(s, log_queue_get_length(s));
  return self->stop(self, persistent);
}

gboolean
log_queue_disk_start(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  /* qdisk portion is not yet started when this happens */
  g_assert(!qdisk_started(self->qdisk));
  g_assert(self->start);

  if (self->start(self))
    {
      log_queue_queued_messages_add(s, log_queue_get_length(s));
      log_queue_disk_update_disk_related_counters(self);
      stats_counter_set(self->metrics.capacity, B_TO_KiB(qdisk_get_max_useful_space(self->qdisk)));
      return TRUE;
    }

  return FALSE;
}

const gchar *
log_queue_disk_get_filename(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  return qdisk_get_filename(self->qdisk);
}

static void
_unregister_counters(LogQueueDisk *self)
{
  stats_lock();
  {
    if (self->metrics.capacity_sc_key)
      {
        stats_unregister_counter(self->metrics.capacity_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.capacity);

        stats_cluster_key_free(self->metrics.capacity_sc_key);
      }

    if (self->metrics.disk_usage_sc_key)
      {
        stats_unregister_counter(self->metrics.disk_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.disk_usage);

        stats_cluster_key_free(self->metrics.disk_usage_sc_key);
      }

    if (self->metrics.disk_allocated_sc_key)
      {
        stats_unregister_counter(self->metrics.disk_allocated_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.disk_allocated);

        stats_cluster_key_free(self->metrics.disk_allocated_sc_key);
      }
  }
  stats_unlock();
}

void
log_queue_disk_free_method(LogQueueDisk *self)
{
  g_assert(!qdisk_started(self->qdisk));
  qdisk_free(self->qdisk);

  _unregister_counters(self);

  log_queue_free_method(&self->super);
}

void
log_queue_disk_update_disk_related_counters(LogQueueDisk *self)
{
  stats_counter_set(self->metrics.disk_usage, B_TO_KiB(qdisk_get_used_useful_space(self->qdisk)));
  stats_counter_set(self->metrics.disk_allocated, B_TO_KiB(qdisk_get_file_size(self->qdisk)));
}

static gboolean
_pop_disk(LogQueueDisk *self, LogMessage **msg)
{
  if (!qdisk_started(self->qdisk))
    return FALSE;

  ScratchBuffersMarker marker;
  GString *read_serialized = scratch_buffers_alloc_and_mark(&marker);

  gint64 read_head = qdisk_get_next_head_position(self->qdisk);

  if (!qdisk_pop_head(self->qdisk, read_serialized))
    {
      msg_error("Cannot read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_int("read_head", read_head));
      scratch_buffers_reclaim_marked(marker);
      return FALSE;
    }

  if (!log_queue_disk_deserialize_msg(self, read_serialized, msg))
    {
      msg_error("Cannot read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_int("read_head", read_head));
      *msg = NULL;
    }

  scratch_buffers_reclaim_marked(marker);

  return TRUE;
}

static gboolean
_peek_disk(LogQueueDisk *self, LogMessage **msg)
{
  if (!qdisk_started(self->qdisk))
    return FALSE;

  ScratchBuffersMarker marker;
  GString *read_serialized = scratch_buffers_alloc_and_mark(&marker);

  gint64 read_head = qdisk_get_next_head_position(self->qdisk);

  if (!qdisk_peek_head(self->qdisk, read_serialized))
    {
      msg_error("Cannot read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_int("read_head", read_head));
      scratch_buffers_reclaim_marked(marker);
      return FALSE;
    }

  if (!log_queue_disk_deserialize_msg(self, read_serialized, msg))
    {
      msg_error("Cannot read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_int("read_head", read_head));
      *msg = NULL;
    }

  scratch_buffers_reclaim_marked(marker);

  return TRUE;
}

LogMessage *
log_queue_disk_read_message(LogQueueDisk *self, LogPathOptions *path_options)
{
  LogMessage *msg = NULL;
  do
    {
      if (qdisk_get_length(self->qdisk) == 0)
        {
          break;
        }
      if (!_pop_disk(self, &msg))
        {
          msg_error("Error reading from disk-queue file, dropping disk queue",
                    evt_tag_str("filename", qdisk_get_filename(self->qdisk)));

          if (!qdisk_is_read_only(self->qdisk))
            log_queue_disk_restart_corrupted(self);

          if (msg)
            log_msg_unref(msg);
          msg = NULL;

          return NULL;
        }
    }
  while (msg == NULL);

  if (msg)
    path_options->ack_needed = FALSE;

  return msg;
}

LogMessage *
log_queue_disk_peek_message(LogQueueDisk *self)
{
  LogMessage *msg = NULL;
  do
    {
      if (qdisk_get_length(self->qdisk) == 0)
        {
          break;
        }
      if (!_peek_disk(self, &msg))
        {
          msg_error("Error reading from disk-queue file, dropping disk queue",
                    evt_tag_str("filename", qdisk_get_filename(self->qdisk)));

          if (!qdisk_is_read_only(self->qdisk))
            log_queue_disk_restart_corrupted(self);

          if (msg)
            log_msg_unref(msg);
          msg = NULL;

          return NULL;
        }
    }
  while (msg == NULL);

  return msg;
}

void
log_queue_disk_drop_message(LogQueueDisk *self, LogMessage *msg, const LogPathOptions *path_options)
{
  log_queue_dropped_messages_inc(&self->super);

  if (path_options->flow_control_requested)
    log_msg_drop(msg, path_options, AT_SUSPENDED);
  else
    log_msg_drop(msg, path_options, AT_PROCESSED);
}

static gchar *
_get_next_corrupted_filename(const gchar *filename)
{
  GString *corrupted_filename = g_string_new(NULL);

  for (gint i = 1; i < 10000; i++)
    {
      if (i == 1)
        g_string_printf(corrupted_filename, "%s.corrupted", filename);
      else
        g_string_printf(corrupted_filename, "%s.corrupted-%d", filename, i);

      struct stat st;
      if (stat(corrupted_filename->str, &st) < 0)
        return g_string_free(corrupted_filename, FALSE);
    }

  msg_error("Failed to calculate filename for corrupted disk-queue",
            evt_tag_str(EVT_TAG_FILENAME, filename));

  return NULL;
}

static void
_restart_diskq(LogQueueDisk *self)
{
  g_assert(self->start);
  g_assert(self->stop);

  const gchar *filename = qdisk_get_filename(self->qdisk);

  if (self->stop_corrupted)
    {
      if (!self->stop_corrupted(self))
        {
          msg_error("Failed to stop corrupted disk-queue-file",
                    evt_tag_str(EVT_TAG_FILENAME, filename));
        }
    }
  else
    {
      gboolean persistent;
      if (!self->stop(self, &persistent))
        {
          msg_error("Failed to stop corrupted disk-queue-file",
                    evt_tag_str(EVT_TAG_FILENAME, filename));
        }
    }

  gchar *new_file = _get_next_corrupted_filename(filename);
  if (!new_file || rename(filename, new_file) < 0)
    {
      msg_error("Moving corrupt disk-queue failed",
                evt_tag_str(EVT_TAG_FILENAME, filename),
                evt_tag_error(EVT_TAG_OSERROR));
    }
  g_free(new_file);

  if (!self->start(self))
    {
      g_assert(FALSE && "Failed to restart a corrupted disk-queue file, baling out.");
    }
}

void
log_queue_disk_restart_corrupted(LogQueueDisk *self)
{
  _restart_diskq(self);
  log_queue_queued_messages_reset(&self->super);
  log_queue_disk_update_disk_related_counters(self);
  stats_counter_set(self->metrics.capacity, B_TO_KiB(qdisk_get_max_useful_space(self->qdisk)));
}

static void
_register_counters(LogQueueDisk *self, gint stats_level, StatsClusterKeyBuilder *builder)
{
  if (!builder)
    return;

  stats_cluster_key_builder_push(builder);
  {
    /* Up to 4 TiB with 32 bit atomic counters. */
    stats_cluster_key_builder_set_unit(builder, SCU_KIB);

    stats_cluster_key_builder_set_name(builder, "capacity_bytes");
    self->metrics.capacity_sc_key = stats_cluster_key_builder_build_single(builder);

    stats_cluster_key_builder_set_name(builder, "disk_usage_bytes");
    self->metrics.disk_usage_sc_key = stats_cluster_key_builder_build_single(builder);

    stats_cluster_key_builder_set_name(builder, "disk_allocated_bytes");
    self->metrics.disk_allocated_sc_key = stats_cluster_key_builder_build_single(builder);
  }
  stats_cluster_key_builder_pop(builder);

  stats_lock();
  {
    stats_register_counter(stats_level, self->metrics.capacity_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.capacity);
    stats_register_counter(stats_level, self->metrics.disk_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.disk_usage);
    stats_register_counter(stats_level, self->metrics.disk_allocated_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.disk_allocated);
  }
  stats_unlock();
}

void
log_queue_disk_init_instance(LogQueueDisk *self, DiskQueueOptions *options, const gchar *qdisk_file_id,
                             const gchar *filename, const gchar *persist_name, gint stats_level,
                             StatsClusterKeyBuilder *driver_sck_builder,
                             StatsClusterKeyBuilder *queue_sck_builder)
{
  if (queue_sck_builder)
    {
      stats_cluster_key_builder_push(queue_sck_builder);
      stats_cluster_key_builder_set_name_prefix(queue_sck_builder, "disk_queue_");
      stats_cluster_key_builder_add_label(queue_sck_builder, stats_cluster_label("path", filename));
      stats_cluster_key_builder_add_label(queue_sck_builder,
                                          stats_cluster_label("reliable", options->reliable ? "true" : "false"));
    }

  log_queue_init_instance(&self->super, persist_name, stats_level, driver_sck_builder, queue_sck_builder);
  self->super.type = log_queue_disk_type;

  self->compaction = options->compaction;

  self->qdisk = qdisk_new(options, qdisk_file_id, filename);
  _register_counters(self, stats_level, queue_sck_builder);

  if (queue_sck_builder)
    stats_cluster_key_builder_pop(queue_sck_builder);
}

static gboolean
_serialize_msg(SerializeArchive *sa, gpointer user_data)
{
  LogQueueDisk *self = ((gpointer *) user_data)[0];
  LogMessage *msg = ((gpointer *) user_data)[1];

  return log_msg_serialize(msg, sa, self->compaction ? LMSF_COMPACTION : 0);
}

gboolean
log_queue_disk_serialize_msg(LogQueueDisk *self, LogMessage *msg, GString *serialized)
{
  gpointer user_data[] = { self, msg };
  GError *error = NULL;

  if (!qdisk_serialize(serialized, _serialize_msg, user_data, &error))
    {
      msg_error("Error serializing message for the disk-queue file",
                evt_tag_str("error", error->message),
                evt_tag_str("persist-name", self->super.persist_name));
      g_error_free(error);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_deserialize_msg(SerializeArchive *sa, gpointer user_data)
{
  LogMessage *msg = user_data;

  return log_msg_deserialize(msg, sa);
}

gboolean
log_queue_disk_deserialize_msg(LogQueueDisk *self, GString *serialized, LogMessage **msg)
{
  LogMessage *local_msg = log_msg_new_empty();
  gpointer user_data = local_msg;
  GError *error = NULL;

  if (!qdisk_deserialize(serialized, _deserialize_msg, user_data, &error))
    {
      msg_error("Error deserializing message from the disk-queue file",
                evt_tag_str("error", error->message),
                evt_tag_str("persist-name", self->super.persist_name));
      log_msg_unref(local_msg);
      g_error_free(error);
      return FALSE;
    }

  *msg = local_msg;

  return TRUE;
}
