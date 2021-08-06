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
  qdisk_stop(self->qdisk);
  qdisk_free(self->qdisk);

  log_queue_free_method(&self->super);
}

static gboolean
_pop_disk(LogQueueDisk *self, LogMessage **msg)
{
  SerializeArchive *sa;

  *msg = NULL;

  if (!qdisk_started(self->qdisk))
    return FALSE;

  ScratchBuffersMarker marker;
  GString *read_serialized = scratch_buffers_alloc_and_mark(&marker);

  if (!qdisk_pop_head(self->qdisk, read_serialized))
    {
      scratch_buffers_reclaim_marked(marker);
      return FALSE;
    }

  sa = serialize_string_archive_new(read_serialized);
  *msg = log_msg_new_empty();

  if (!log_msg_deserialize(*msg, sa))
    {
      serialize_archive_free(sa);
      log_msg_unref(*msg);
      scratch_buffers_reclaim_marked(marker);
      *msg = NULL;
      msg_error("Can't read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_long("read_position", qdisk_get_reader_head(self->qdisk)));
      return TRUE;
    }

  serialize_archive_free(sa);
  scratch_buffers_reclaim_marked(marker);

  return TRUE;
}

LogMessage *
log_queue_disk_read_message(LogQueueDisk *self, LogPathOptions *path_options)
{
  LogMessage *msg = NULL;
  do
    {
      if (qdisk_get_length (self->qdisk) == 0)
        {
          break;
        }
      if (!_pop_disk (self, &msg))
        {
          msg_error("Error reading from disk-queue file, dropping disk queue",
                    evt_tag_str("filename", qdisk_get_filename(self->qdisk)));
          log_queue_disk_restart_corrupted(self);
          if (msg)
            log_msg_unref (msg);
          msg = NULL;
          return NULL;
        }
    }
  while (msg == NULL);
  return msg;
}

gboolean
log_queue_disk_write_message(LogQueueDisk *self, LogMessage *msg)
{
  SerializeArchive *sa;
  DiskQueueOptions *options = qdisk_get_options(self->qdisk);
  gboolean consumed = FALSE;

  if (qdisk_started(self->qdisk) && qdisk_is_space_avail(self->qdisk, 64))
    {
      ScratchBuffersMarker marker;
      GString *write_serialized = scratch_buffers_alloc_and_mark(&marker);

      sa = serialize_string_archive_new(write_serialized);
      log_msg_serialize(msg, sa, options->compaction ? LMSF_COMPACTION : 0);
      consumed = qdisk_push_tail(self->qdisk, write_serialized);
      serialize_archive_free(sa);

      scratch_buffers_reclaim_marked(marker);
    }
  return consumed;
}

static void
_restart_diskq(LogQueueDisk *self)
{
  gchar *filename = g_strdup(qdisk_get_filename(self->qdisk));
  DiskQueueOptions *options = qdisk_get_options(self->qdisk);

  qdisk_stop(self->qdisk);

  gchar *new_file = g_strdup_printf("%s.corrupted", filename);
  if (rename(filename, new_file) < 0)
    {
      msg_error("Moving corrupt disk-queue failed",
                evt_tag_str(EVT_TAG_FILENAME, filename),
                evt_tag_error(EVT_TAG_OSERROR));
    }
  g_free(new_file);

  if (self->restart)
    self->restart(self, options);

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
}


void
log_queue_disk_init_instance(LogQueueDisk *self, DiskQueueOptions *options, const gchar *qdisk_file_id,
                             const gchar *persist_name)
{
  log_queue_init_instance(&self->super, persist_name);
  self->super.type = log_queue_disk_type;

  self->qdisk = qdisk_new(options, qdisk_file_id);
}
