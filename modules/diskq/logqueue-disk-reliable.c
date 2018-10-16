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

#include "syslog-ng.h"
#include "qdisk.h"
#include "logpipe.h"
#include "logqueue-disk-reliable.h"
#include "messages.h"

/*pessimistic default for reliable disk queue 10000 x 16 kbyte*/
#define PESSIMISTIC_MEM_BUF_SIZE 10000 * 16 *1024

static gboolean
_start(LogQueueDisk *s, const gchar *filename)
{
  return qdisk_start(s->qdisk, filename, NULL, NULL, NULL);
}

static gboolean
_skip_message(LogQueueDisk *self)
{
  GString *serialized;
  SerializeArchive *sa;

  if (!qdisk_started(self->qdisk))
    return FALSE;

  serialized = g_string_sized_new(64);
  if (!qdisk_pop_head(self->qdisk, serialized))
    {
      g_string_free(serialized, TRUE);
      return FALSE;
    }

  sa = serialize_string_archive_new(serialized);
  serialize_archive_free(sa);

  g_string_free(serialized, TRUE);
  return TRUE;
}

static void
_empty_queue(GQueue *self)
{
  while (self && self->length > 0)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      gint64 *temppos = g_queue_pop_head(self);
      LogMessage *msg = g_queue_pop_head(self);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self), &path_options);

      g_free(temppos);
      log_msg_drop(msg, &path_options, AT_PROCESSED);
    }
}

static gint64
_get_length(LogQueueDisk *self)
{
  return qdisk_get_length(self->qdisk);
}

static void
_ack_backlog(LogQueueDisk *s, guint num_msg_to_ack)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  guint i;

  for (i = 0; i < num_msg_to_ack; i++)
    {
      gint64 pos;
      if (qdisk_get_backlog_head (self->super.qdisk) == qdisk_get_reader_head (self->super.qdisk))
        {
          goto exit_reliable;
        }
      if (self->qbacklog->length > 0)
        {
          gint64 *temppos = g_queue_pop_head (self->qbacklog);
          pos = *temppos;
          if (pos == qdisk_get_backlog_head (self->super.qdisk))
            {
              msg = g_queue_pop_head (self->qbacklog);
              POINTER_TO_LOG_PATH_OPTIONS (g_queue_pop_head (self->qbacklog), &path_options);
              log_msg_ack (msg, &path_options, AT_PROCESSED);
              log_msg_unref (msg);
              g_free (temppos);
            }
          else
            {
              g_queue_push_head (self->qbacklog, temppos);
            }
        }
      guint64 new_backlog = qdisk_get_backlog_head (self->super.qdisk);
      new_backlog = qdisk_skip_record(self->super.qdisk, new_backlog);
      qdisk_set_backlog_head (self->super.qdisk, new_backlog);
      qdisk_dec_backlog (self->super.qdisk);
    }
exit_reliable:
  qdisk_reset_file_if_possible (self->super.qdisk);
}

static gint
_find_pos_in_qbacklog(LogQueueDiskReliable *self, gint64 new_pos)
{
  gint result = -1;
  int i = 0;
  GList *item = self->qbacklog->tail;
  while (result == -1 && item != NULL)
    {
      GList *pos_i = item->prev->prev;
      gint64 *pos = pos_i->data;
      if (*pos == new_pos)
        {
          result = i;
        }
      item = pos_i->prev;
      i++;
    }
  return result;
}

static void
_move_message_from_qbacklog_to_qreliable(LogQueueDiskReliable *self)
{
  gpointer ptr_opt = g_queue_pop_tail(self->qbacklog);
  gpointer ptr_msg = g_queue_pop_tail(self->qbacklog);
  gpointer ptr_pos = g_queue_pop_tail(self->qbacklog);

  g_queue_push_head(self->qreliable, ptr_opt);
  g_queue_push_head(self->qreliable, ptr_msg);
  g_queue_push_head(self->qreliable, ptr_pos);

  log_queue_memory_usage_add(&self->super.super, log_msg_get_size(ptr_msg));
}

static void
_rewind_from_qbacklog(LogQueueDiskReliable *self, gint64 new_pos)
{
  gint i;
  g_assert((self->qbacklog->length % 3) == 0);

  gint rewind_backlog_queue = _find_pos_in_qbacklog(self, new_pos);
  for (i = 0; i <= rewind_backlog_queue; i++)
    {
      _move_message_from_qbacklog_to_qreliable(self);
    }
}


static void
_rewind_backlog(LogQueueDisk *s, guint rewind_count)
{
  guint i;

  guint number_of_messages_stay_in_backlog;
  gint64 new_read_head;
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  rewind_count = MIN(rewind_count, qdisk_get_backlog_count (self->super.qdisk));
  number_of_messages_stay_in_backlog = qdisk_get_backlog_count (self->super.qdisk) - rewind_count;
  new_read_head = qdisk_get_backlog_head (self->super.qdisk);
  for (i = 0; i < number_of_messages_stay_in_backlog; i++)
    {
      new_read_head = qdisk_skip_record(self->super.qdisk, new_read_head);
    }
  _rewind_from_qbacklog(self, new_read_head);

  qdisk_set_backlog_count (self->super.qdisk, number_of_messages_stay_in_backlog);
  qdisk_set_reader_head (self->super.qdisk, new_read_head);
  qdisk_set_length (self->super.qdisk, qdisk_get_length (self->super.qdisk) + rewind_count);

  log_queue_queued_messages_add(&self->super.super, rewind_count);
}

static LogMessage *
_pop_head(LogQueueDisk *s, LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  LogMessage *msg = NULL;
  if (self->qreliable->length > 0)
    {
      gint64 *temppos = g_queue_pop_head (self->qreliable);
      gint64 pos = *temppos;
      if (pos == qdisk_get_reader_head (self->super.qdisk))
        {
          msg = g_queue_pop_head (self->qreliable);
          log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));

          POINTER_TO_LOG_PATH_OPTIONS (g_queue_pop_head (self->qreliable), path_options);
          _skip_message (s);
          if (self->super.super.use_backlog)
            {
              log_msg_ref (msg);
              g_queue_push_tail (self->qbacklog, temppos);
              g_queue_push_tail (self->qbacklog, msg);
              g_queue_push_tail (self->qbacklog, LOG_PATH_OPTIONS_TO_POINTER (path_options));
            }
          else
            {
              g_free (temppos);
            }
        }
      else
        {
          /* try to write the message to the disk */
          g_queue_push_head (self->qreliable, temppos);
        }
    }

  if (msg == NULL)
    {
      msg = s->read_message(s, path_options);
      if (msg)
        {
          path_options->ack_needed = FALSE;
        }
    }

  if (msg != NULL)
    {
      if (self->super.super.use_backlog)
        {
          qdisk_inc_backlog(self->super.qdisk);
        }
      else
        {
          qdisk_set_backlog_head(self->super.qdisk, qdisk_get_reader_head(self->super.qdisk));
        }
    }
  return msg;
}

static gboolean
_push_tail(LogQueueDisk *s, LogMessage *msg, LogPathOptions *local_options, const LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;

  gint64 last_wpos = qdisk_get_writer_head (self->super.qdisk);
  if (!s->write_message(s, msg))
    {
      /* we were not able to store the msg, warn */
      msg_error("Destination reliable queue full, dropping message",
                evt_tag_str("filename", qdisk_get_filename (self->super.qdisk)),
                evt_tag_long("queue_len", _get_length(s)),
                evt_tag_int("mem_buf_size", qdisk_get_memory_size (self->super.qdisk)),
                evt_tag_long("disk_buf_size", qdisk_get_size (self->super.qdisk)),
                evt_tag_str("persist_name", self->super.super.persist_name));

      return FALSE;
    }

  /* check the remaining space: if it is less than the mem_buf_size, the message cannot be acked */
  if (qdisk_get_empty_space(self->super.qdisk) < qdisk_get_memory_size (self->super.qdisk))
    {
      /* we have reached the reserved buffer size, keep the msg in memory
       * the message is written but into the overflow area
       */
      gint64 *temppos = g_malloc (sizeof(gint64));
      *temppos = last_wpos;
      g_queue_push_tail (self->qreliable, temppos);
      g_queue_push_tail (self->qreliable, msg);
      g_queue_push_tail (self->qreliable, LOG_PATH_OPTIONS_TO_POINTER(path_options));
      log_msg_ref (msg);

      log_queue_memory_usage_add(&self->super.super, log_msg_get_size(msg));
      local_options->ack_needed = FALSE;
    }

  return TRUE;
}

static void
_free_queue(LogQueueDisk *s)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  _empty_queue(self->qreliable);
  _empty_queue(self->qbacklog);
  g_queue_free(self->qreliable);
  self->qreliable = NULL;
  g_queue_free(self->qbacklog);
  self->qbacklog = NULL;
}

static gboolean
_load_queue(LogQueueDisk *s, const gchar *filename)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  _empty_queue(self->qreliable);
  return qdisk_start(s->qdisk, filename, NULL, NULL, NULL);
}

static gboolean
_save_queue (LogQueueDisk *s, gboolean *persistent)
{
  *persistent = TRUE;
  qdisk_deinit (s->qdisk);
  return TRUE;
}

static void
_restart(LogQueueDisk *s, DiskQueueOptions *options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  qdisk_init_instance(self->super.qdisk, options, "SLRQ");
}


static void
_set_virtual_functions(LogQueueDisk *self)
{
  self->get_length = _get_length;
  self->ack_backlog = _ack_backlog;
  self->rewind_backlog = _rewind_backlog;
  self->pop_head = _pop_head;
  self->push_tail = _push_tail;
  self->free_fn = _free_queue;
  self->load_queue = _load_queue;
  self->start = _start;
  self->save_queue = _save_queue;
  self->restart = _restart;
}

LogQueue *
log_queue_disk_reliable_new(DiskQueueOptions *options, const gchar *persist_name)
{
  g_assert(options->reliable == TRUE);
  LogQueueDiskReliable *self = g_new0(LogQueueDiskReliable, 1);
  log_queue_disk_init_instance(&self->super, persist_name);
  qdisk_init_instance(self->super.qdisk, options, "SLRQ");
  if (options->mem_buf_size < 0)
    {
      options->mem_buf_size = PESSIMISTIC_MEM_BUF_SIZE;
    }
  self->qreliable = g_queue_new();
  self->qbacklog = g_queue_new();
  _set_virtual_functions(&self->super);
  return &self->super.super;
}
