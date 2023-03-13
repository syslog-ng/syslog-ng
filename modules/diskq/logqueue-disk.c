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

QueueType log_queue_disk_type = "DISK";

gboolean
log_queue_disk_save_queue(LogQueue *s, gboolean *persistent)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  if (!qdisk_started(self->qdisk))
    {
      *persistent = FALSE;
      return TRUE;
    }

  if (self->save_queue)
    return self->save_queue(self, persistent);
  return FALSE;
}

gboolean
log_queue_disk_load_queue(LogQueue *s, const gchar *filename)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  /* qdisk portion is not yet started when this happens */
  g_assert(!qdisk_started(self->qdisk));

  if (self->load_queue)
    return self->load_queue(self, filename);
  return FALSE;
}

const gchar *
log_queue_disk_get_filename(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  return qdisk_get_filename(self->qdisk);
}

void
log_queue_disk_free_method(LogQueueDisk *self)
{
  g_assert(!qdisk_started(self->qdisk));
  qdisk_free(self->qdisk);

  log_queue_free_method(&self->super);
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

void
log_queue_disk_drop_message(LogQueueDisk *self, LogMessage *msg, const LogPathOptions *path_options)
{
  stats_counter_inc(self->super.dropped_messages);

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
  gchar *filename = g_strdup(qdisk_get_filename(self->qdisk));

  gboolean persistent;
  if (self->save_queue)
    self->save_queue(self, &persistent);

  gchar *new_file = _get_next_corrupted_filename(filename);
  if (!new_file || rename(filename, new_file) < 0)
    {
      msg_error("Moving corrupt disk-queue failed",
                evt_tag_str(EVT_TAG_FILENAME, filename),
                evt_tag_error(EVT_TAG_OSERROR));
    }
  g_free(new_file);

  if (self->start)
    {
      self->start(self, filename);
    }
  g_free(filename);
}

void
log_queue_disk_restart_corrupted(LogQueueDisk *self)
{
  _restart_diskq(self);
  log_queue_queued_messages_reset(&self->super);
}


void
log_queue_disk_init_instance(LogQueueDisk *self, DiskQueueOptions *options, const gchar *qdisk_file_id,
                             const gchar *persist_name)
{
  log_queue_init_instance(&self->super, persist_name);
  self->super.type = log_queue_disk_type;

  self->compaction = options->compaction;

  self->qdisk = qdisk_new(options, qdisk_file_id);
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
