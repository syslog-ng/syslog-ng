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
#include "scratch-buffers.h"

/*pessimistic default for reliable disk queue 10000 x 16 kbyte*/
#define PESSIMISTIC_MEM_BUF_SIZE 10000 * 16 *1024

static inline void
_push_to_memory_queue_tail(GQueue *queue, gint64 *position, LogMessage *msg, const LogPathOptions *path_options)
{
  g_queue_push_tail(queue, position);
  g_queue_push_tail(queue, msg);
  g_queue_push_tail(queue, LOG_PATH_OPTIONS_TO_POINTER(path_options));
}

static gboolean
_start(LogQueueDisk *s, const gchar *filename)
{
  return qdisk_start(s->qdisk, filename, NULL, NULL, NULL);
}

static gboolean
_skip_message(LogQueueDisk *self)
{
  if (!qdisk_started(self->qdisk))
    return FALSE;

  return qdisk_remove_head(self->qdisk);
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
_get_length(LogQueue *s)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  if (!qdisk_started(self->super.qdisk))
    return 0;

  return qdisk_get_length(self->super.qdisk);
}

static void
_ack_backlog(LogQueue *s, gint num_msg_to_ack)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  guint i;

  g_static_mutex_lock(&s->lock);

  for (i = 0; i < num_msg_to_ack; i++)
    {
      gint64 pos;
      if (qdisk_get_backlog_head(self->super.qdisk) == qdisk_get_head_position(self->super.qdisk))
        {
          goto exit_reliable;
        }
      if (self->qbacklog->length > 0)
        {
          gint64 *temppos = g_queue_pop_head(self->qbacklog);
          pos = *temppos;
          if (pos == qdisk_get_backlog_head(self->super.qdisk))
            {
              msg = g_queue_pop_head(self->qbacklog);
              POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
              log_msg_ack(msg, &path_options, AT_PROCESSED);
              log_msg_unref(msg);
              g_free(temppos);
            }
          else
            {
              g_queue_push_head(self->qbacklog, temppos);
            }
        }
      guint64 new_backlog = qdisk_get_backlog_head(self->super.qdisk);
      new_backlog = qdisk_skip_record(self->super.qdisk, new_backlog);
      qdisk_set_backlog_head(self->super.qdisk, new_backlog);
      qdisk_dec_backlog(self->super.qdisk);
    }
exit_reliable:
  qdisk_reset_file_if_empty(self->super.qdisk);
  g_static_mutex_unlock(&s->lock);
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
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  guint i;

  guint number_of_messages_stay_in_backlog;
  gint64 new_read_head;
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  g_static_mutex_lock(&s->lock);

  rewind_count = MIN(rewind_count, qdisk_get_backlog_count(self->super.qdisk));
  number_of_messages_stay_in_backlog = qdisk_get_backlog_count(self->super.qdisk) - rewind_count;
  new_read_head = qdisk_get_backlog_head(self->super.qdisk);
  for (i = 0; i < number_of_messages_stay_in_backlog; i++)
    {
      new_read_head = qdisk_skip_record(self->super.qdisk, new_read_head);
    }
  _rewind_from_qbacklog(self, new_read_head);

  qdisk_set_backlog_count(self->super.qdisk, number_of_messages_stay_in_backlog);
  qdisk_set_reader_head(self->super.qdisk, new_read_head);
  qdisk_set_length(self->super.qdisk, qdisk_get_length(self->super.qdisk) + rewind_count);

  log_queue_queued_messages_add(s, rewind_count);

  g_static_mutex_unlock(&s->lock);
}

static void
_rewind_backlog_all(LogQueue *s)
{
  _rewind_backlog(s, -1);
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  g_static_mutex_lock(&s->lock);

  LogMessage *msg = NULL;
  if (self->qreliable->length > 0)
    {
      gint64 *temppos = g_queue_pop_head(self->qreliable);
      gint64 pos = *temppos;
      if (pos == qdisk_get_head_position(self->super.qdisk))
        {
          msg = g_queue_pop_head(self->qreliable);
          log_queue_memory_usage_sub(s, log_msg_get_size(msg));

          POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qreliable), path_options);
          _skip_message(&self->super);
          if (s->use_backlog)
            {
              log_msg_ref(msg);
              _push_to_memory_queue_tail(self->qbacklog, temppos, msg, path_options);
            }
          else
            {
              g_free(temppos);
            }
        }
      else
        {
          /* try to write the message to the disk */
          g_queue_push_head(self->qreliable, temppos);
        }
    }

  if (msg == NULL)
    {
      msg = log_queue_disk_read_message(&self->super, path_options);
      if (msg)
        {
          path_options->ack_needed = FALSE;
        }
    }

  if (msg != NULL)
    {
      if (s->use_backlog)
        {
          qdisk_inc_backlog(self->super.qdisk);
        }
      else
        {
          qdisk_set_backlog_head(self->super.qdisk, qdisk_get_head_position(self->super.qdisk));
        }
      log_queue_queued_messages_dec(s);
    }

  g_static_mutex_unlock(&s->lock);

  return msg;
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  ScratchBuffersMarker marker;
  GString *serialized_msg = scratch_buffers_alloc_and_mark(&marker);
  if (!qdisk_serialize_msg(self->super.qdisk, msg, serialized_msg))
    {
      msg_error("Failed to serialize message for reliable disk-buffer, dropping message",
                evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                evt_tag_str("persist_name", s->persist_name));
      log_queue_disk_drop_message(&self->super, msg, path_options);
      scratch_buffers_reclaim_marked(marker);
      return;
    }

  g_static_mutex_lock(&s->lock);

  LogPathOptions local_options = *path_options;

  gint64 message_position = qdisk_get_next_tail_position(self->super.qdisk);
  if (!qdisk_push_tail(self->super.qdisk, serialized_msg))
    {
      /* we were not able to store the msg, warn */
      msg_error("Destination reliable queue full, dropping message",
                evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                evt_tag_long("queue_len", log_queue_get_length(s)),
                evt_tag_int("mem_buf_size", qdisk_get_memory_size(self->super.qdisk)),
                evt_tag_long("disk_buf_size", qdisk_get_maximum_size(self->super.qdisk)),
                evt_tag_str("persist_name", s->persist_name));
      log_queue_disk_drop_message(&self->super, msg, path_options);
      scratch_buffers_reclaim_marked(marker);
      g_static_mutex_unlock(&s->lock);
      return;
    }

  scratch_buffers_reclaim_marked(marker);

  /* check the remaining space: if it is less than the mem_buf_size, the message cannot be acked */
  if (qdisk_get_empty_space(self->super.qdisk) < qdisk_get_memory_size(self->super.qdisk))
    {
      /* we have reached the reserved buffer size, keep the msg in memory
       * the message is written but into the overflow area
       */
      gint64 *temppos = g_malloc(sizeof(gint64));
      *temppos = message_position;
      log_msg_ref(msg);
      _push_to_memory_queue_tail(self->qreliable, temppos, msg, path_options);

      log_queue_memory_usage_add(s, log_msg_get_size(msg));
      local_options.ack_needed = FALSE;
    }

  log_queue_push_notify(s);
  log_queue_queued_messages_inc(s);
  log_msg_ack(msg, &local_options, AT_PROCESSED);
  log_msg_unref(msg);

  g_static_mutex_unlock(&s->lock);
}

static void
_push_head(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  g_assert_not_reached();
}

static void
_free(LogQueue *s)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  _empty_queue(self->qreliable);
  _empty_queue(self->qbacklog);
  g_queue_free(self->qreliable);
  self->qreliable = NULL;
  g_queue_free(self->qbacklog);
  self->qbacklog = NULL;

  log_queue_disk_free_method(&self->super);
}

static gboolean
_load_queue(LogQueueDisk *s, const gchar *filename)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  _empty_queue(self->qreliable);
  return qdisk_start(s->qdisk, filename, NULL, NULL, NULL);
}

static gboolean
_save_queue(LogQueueDisk *s, gboolean *persistent)
{
  *persistent = TRUE;
  qdisk_stop(s->qdisk);
  return TRUE;
}

static void
_restart(LogQueueDisk *s, DiskQueueOptions *options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;
  qdisk_init_instance(self->super.qdisk, options, "SLRQ");
}

static inline void
_set_logqueue_virtual_functions(LogQueue *s)
{
  s->get_length = _get_length;
  s->ack_backlog = _ack_backlog;
  s->rewind_backlog = _rewind_backlog;
  s->rewind_backlog_all = _rewind_backlog_all;
  s->pop_head = _pop_head;
  s->push_tail = _push_tail;
  s->push_head = _push_head;
  s->free_fn = _free;
}

static inline void
_set_logqueue_disk_virtual_functions(LogQueueDisk *s)
{
  s->load_queue = _load_queue;
  s->start = _start;
  s->save_queue = _save_queue;
  s->restart = _restart;
}

static inline void
_set_virtual_functions(LogQueueDiskReliable *self)
{
  _set_logqueue_virtual_functions(&self->super.super);
  _set_logqueue_disk_virtual_functions(&self->super);
}

LogQueue *
log_queue_disk_reliable_new(DiskQueueOptions *options, const gchar *persist_name)
{
  g_assert(options->reliable == TRUE);
  LogQueueDiskReliable *self = g_new0(LogQueueDiskReliable, 1);
  log_queue_disk_init_instance(&self->super, options, "SLRQ", persist_name);
  if (options->mem_buf_size < 0)
    {
      options->mem_buf_size = PESSIMISTIC_MEM_BUF_SIZE;
    }
  self->qreliable = g_queue_new();
  self->qbacklog = g_queue_new();
  _set_virtual_functions(self);
  return &self->super.super;
}
