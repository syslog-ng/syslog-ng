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

static gint64
_get_length(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  gint64 qdisk_length = 0;

  if (qdisk_started(self->qdisk) && self->get_length)
    {
      qdisk_length = self->get_length(self);
    }
  return qdisk_length;
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  LogPathOptions local_options = *path_options;
  g_static_mutex_lock(&self->super.lock);
  if (self->push_tail)
    {
      if (self->push_tail(self, msg, &local_options, path_options))
        {
          log_queue_push_notify (&self->super);
          log_queue_queued_messages_inc(&self->super);
          log_msg_ack(msg, &local_options, AT_PROCESSED);
          log_msg_unref(msg);
          g_static_mutex_unlock(&self->super.lock);
          return;
        }
    }
  stats_counter_inc (self->super.dropped_messages);

  if (path_options->flow_control_requested)
    log_msg_ack(msg, path_options, AT_SUSPENDED);
  else
    log_msg_drop(msg, path_options, AT_PROCESSED);

  g_static_mutex_unlock(&self->super.lock);
}

static void
_push_head(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  g_static_mutex_lock(&self->super.lock);
  if (self->push_head)
    {
      self->push_head(self, msg, path_options);
    }
  g_static_mutex_unlock(&self->super.lock);
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  LogMessage *msg = NULL;

  msg = NULL;
  g_static_mutex_lock(&self->super.lock);
  if (self->pop_head)
    {
      msg = self->pop_head(self, path_options);
    }
  if (msg != NULL)
    {
      log_queue_queued_messages_dec(&self->super);
    }
  g_static_mutex_unlock(&self->super.lock);
  return msg;
}

static void
_ack_backlog(LogQueue *s, gint num_msg_to_ack)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  g_static_mutex_lock(&self->super.lock);

  if (self->ack_backlog)
    {
      self->ack_backlog(self, num_msg_to_ack);
    }

  g_static_mutex_unlock(&self->super.lock);
}

static void
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  g_static_mutex_lock(&self->super.lock);

  if (self->rewind_backlog)
    {
      self->rewind_backlog (self, rewind_count);
    }

  g_static_mutex_unlock(&self->super.lock);
}

void
_backlog_all(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  g_static_mutex_lock(&self->super.lock);

  if (self->rewind_backlog)
    {
      self->rewind_backlog(self, -1);
    }

  g_static_mutex_unlock(&self->super.lock);
}

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

gboolean
log_queue_disk_is_reliable(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  if (self->is_reliable)
    return self->is_reliable(self);
  return FALSE;
}

const gchar *
log_queue_disk_get_filename(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;
  return qdisk_get_filename(self->qdisk);
}

static void
_free(LogQueue *s)
{
  LogQueueDisk *self = (LogQueueDisk *) s;

  if (self->free_fn)
    self->free_fn(self);

  qdisk_stop(self->qdisk);
  qdisk_free(self->qdisk);

  log_queue_free_method(s);
}

static gboolean
_pop_disk(LogQueueDisk *self, LogMessage **msg)
{
  GString *serialized;
  SerializeArchive *sa;

  *msg = NULL;

  if (!qdisk_started(self->qdisk))
    return FALSE;

  serialized = g_string_sized_new(64);
  if (!qdisk_pop_head(self->qdisk, serialized))
    {
      g_string_free(serialized, TRUE);
      return FALSE;
    }

  sa = serialize_string_archive_new(serialized);
  *msg = log_msg_new_empty();

  if (!log_msg_deserialize(*msg, sa))
    {
      g_string_free(serialized, TRUE);
      serialize_archive_free(sa);
      log_msg_unref(*msg);
      *msg = NULL;
      msg_error("Can't read correct message from disk-queue file",
                evt_tag_str("filename", qdisk_get_filename(self->qdisk)),
                evt_tag_long("read_position", qdisk_get_reader_head(self->qdisk)));
      return TRUE;
    }

  serialize_archive_free(sa);

  g_string_free(serialized, TRUE);
  return TRUE;
}

static LogMessage *
_read_message(LogQueueDisk *self, LogPathOptions *path_options)
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

static gboolean
_write_message(LogQueueDisk *self, LogMessage *msg)
{
  GString *serialized;
  SerializeArchive *sa;
  gboolean consumed = FALSE;
  if (qdisk_started(self->qdisk) && qdisk_is_space_avail(self->qdisk, 64))
    {
      serialized = g_string_sized_new(64);
      sa = serialize_string_archive_new(serialized);
      log_msg_serialize(msg, sa);
      consumed = qdisk_push_tail(self->qdisk, serialized);
      serialize_archive_free(sa);
      g_string_free(serialized, TRUE);
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
log_queue_disk_init_instance(LogQueueDisk *self, const gchar *persist_name)
{
  log_queue_init_instance(&self->super, persist_name);
  self->qdisk = qdisk_new();

  self->super.type = log_queue_disk_type;
  self->super.get_length = _get_length;
  self->super.push_tail = _push_tail;
  self->super.push_head = _push_head;
  self->super.pop_head = _pop_head;
  self->super.ack_backlog = _ack_backlog;
  self->super.rewind_backlog = _rewind_backlog;
  self->super.rewind_backlog_all = _backlog_all;
  self->super.free_fn = _free;

  self->read_message = _read_message;
  self->write_message = _write_message;
}
