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

#include "logqueue-disk-non-reliable.h"
#include "logpipe.h"
#include "messages.h"
#include "syslog-ng.h"
#include "scratch-buffers.h"

#define ITEM_NUMBER_PER_MESSAGE 2

static void
_update_memory_usage_during_load(LogQueueDiskNonReliable *s, GQueue *memory_queue, guint offset)
{
  if (g_queue_get_length(memory_queue) == offset)
    return;

  GList *first_link = g_queue_peek_nth_link(memory_queue, offset);
  for (GList *msg_link = first_link; msg_link; msg_link = msg_link->next->next)
    {
      LogMessage *msg = (LogMessage *) msg_link->data;
      log_queue_memory_usage_add(&s->super.super, log_msg_get_size(msg));
    }
}

static gboolean
_start(LogQueueDisk *s, const gchar *filename)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *) s;

  guint qout_length_before_start = g_queue_get_length(self->qout);
  guint qbacklog_length_before_start = g_queue_get_length(self->qbacklog);
  guint qoverflow_length_before_start = g_queue_get_length(self->qoverflow);

  gboolean retval = qdisk_start(s->qdisk, filename, self->qout, self->qbacklog, self->qoverflow);

  _update_memory_usage_during_load(self, self->qout, qout_length_before_start);
  _update_memory_usage_during_load(self, self->qbacklog, qbacklog_length_before_start);
  _update_memory_usage_during_load(self, self->qoverflow, qoverflow_length_before_start);

  return retval;
}

static inline guint
_get_message_number_in_queue(GQueue *queue)
{
  return queue->length / ITEM_NUMBER_PER_MESSAGE;
}

#define HAS_SPACE_IN_QUEUE(queue) (_get_message_number_in_queue(queue) < queue ## _size)

static gint64
_get_length(LogQueue *s)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;

  if (!qdisk_started(self->super.qdisk))
    return 0;

  return _get_message_number_in_queue(self->qout)
         + qdisk_get_length(self->super.qdisk)
         + _get_message_number_in_queue(self->qoverflow);
}

static inline gboolean
_can_push_to_qout(LogQueueDiskNonReliable *self)
{
  return HAS_SPACE_IN_QUEUE(self->qout) && qdisk_get_length(self->super.qdisk) == 0;
}

static inline gboolean
_qoverflow_has_movable_message(LogQueueDiskNonReliable *self)
{
  return self->qoverflow->length > 0
         && (_can_push_to_qout(self) || qdisk_is_space_avail(self->super.qdisk, 4096));
}

static gboolean
_serialize_and_write_message_to_disk(LogQueueDiskNonReliable *self, LogMessage *msg)
{
  ScratchBuffersMarker marker;
  GString *write_serialized = scratch_buffers_alloc_and_mark(&marker);
  if (!log_queue_disk_serialize_msg(&self->super, msg, write_serialized))
    {
      scratch_buffers_reclaim_marked(marker);
      return FALSE;
    }

  gboolean success = qdisk_push_tail(self->super.qdisk, write_serialized);

  scratch_buffers_reclaim_marked(marker);
  return success;
}

static void
_move_messages_from_overflow(LogQueueDiskNonReliable *self)
{
  LogMessage *msg;
  LogPathOptions path_options;
  /* move away as much entries from the overflow area as possible */
  while (_qoverflow_has_movable_message(self))
    {
      msg = g_queue_pop_head(self->qoverflow);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qoverflow), &path_options);

      if (_can_push_to_qout(self))
        {
          /* we can skip qdisk, go straight to qout */
          g_queue_push_tail(self->qout, msg);
          g_queue_push_tail(self->qout, LOG_PATH_OPTIONS_FOR_BACKLOG);
          log_msg_ack(msg, &path_options, AT_PROCESSED);
        }
      else
        {
          if (_serialize_and_write_message_to_disk(self, msg))
            {
              log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));
              log_msg_ack(msg, &path_options, AT_PROCESSED);
              log_msg_unref(msg);
            }
          else
            {
              /* oops, although there seemed to be some free space available,
               * we failed saving this message, (it might have needed more
               * than 4096 bytes than we ensured), push back and break
               */
              g_queue_push_head(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(&path_options));
              g_queue_push_head(self->qoverflow, msg);
              break;
            }
        }
    }
}

static gboolean
_move_messages_from_disk_to_qout(LogQueueDiskNonReliable *self)
{
  do
    {
      if (qdisk_get_length(self->super.qdisk) <= 0)
        break;

      LogPathOptions path_options;
      LogMessage *msg = log_queue_disk_read_message(&self->super, &path_options);

      if (!msg)
        return FALSE;

      g_queue_push_tail(self->qout, msg);
      g_queue_push_tail(self->qout, LOG_PATH_OPTIONS_TO_POINTER(&path_options));
      log_queue_memory_usage_add(&self->super.super, log_msg_get_size(msg));
    }
  while (HAS_SPACE_IN_QUEUE(self->qout));

  return TRUE;
}

static inline gboolean
_maybe_move_messages_among_queue_segments(LogQueueDiskNonReliable *self)
{
  gboolean ret = TRUE;

  if (qdisk_is_read_only(self->super.qdisk))
    return TRUE;

  if (self->qout->length == 0 && self->qout_size > 0)
    ret = _move_messages_from_disk_to_qout(self);

  if (self->qoverflow->length > 0)
    _move_messages_from_overflow(self);

  return ret;
}

/* runs only in the output thread */
static void
_ack_backlog(LogQueue *s, gint num_msg_to_ack)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  guint i;

  for (i = 0; i < num_msg_to_ack; i++)
    {
      if (self->qbacklog->length < ITEM_NUMBER_PER_MESSAGE)
        return;
      msg = g_queue_pop_head(self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
      log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));
      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
    }
}

static void
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  guint i;
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;

  g_mutex_lock(&s->lock);

  rewind_count = MIN(rewind_count, _get_message_number_in_queue(self->qbacklog));

  for (i = 0; i < rewind_count; i++)
    {
      gpointer ptr_opt = g_queue_pop_tail(self->qbacklog);
      gpointer ptr_msg = g_queue_pop_tail(self->qbacklog);

      g_queue_push_head(self->qout, ptr_opt);
      g_queue_push_head(self->qout, ptr_msg);

      log_queue_queued_messages_inc(s);
    }

  g_mutex_unlock(&s->lock);
}

static void
_rewind_backlog_all(LogQueue *s)
{
  _rewind_backlog(s, G_MAXUINT);
}

static inline LogMessage *
_pop_head_qout(LogQueueDiskNonReliable *self, LogPathOptions *path_options)
{
  LogMessage *msg = g_queue_pop_head(self->qout);
  POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qout), path_options);
  log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));

  return msg;
}

static inline LogMessage *
_pop_head_qoverflow(LogQueueDiskNonReliable *self, LogPathOptions *path_options)
{
  LogMessage *msg = g_queue_pop_head(self->qoverflow);
  POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qoverflow), path_options);
  log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));

  return msg;
}

/* runs only in the output thread */
static inline void
_push_tail_qbacklog(LogQueueDiskNonReliable *self, LogMessage *msg, LogPathOptions *path_options)
{
  log_msg_ref(msg);
  g_queue_push_tail(self->qbacklog, msg);
  g_queue_push_tail(self->qbacklog, LOG_PATH_OPTIONS_TO_POINTER(path_options));
  log_queue_memory_usage_add(&self->super.super, log_msg_get_size(msg));
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;
  LogMessage *msg = NULL;
  gboolean stats_update = TRUE;

  g_mutex_lock(&s->lock);

  if (self->qout->length > 0)
    {
      msg = _pop_head_qout(self, path_options);
      if (msg)
        goto success;
    }

  msg = log_queue_disk_read_message(&self->super, path_options);
  if (msg)
    goto success;

  if (self->qoverflow->length > 0 && qdisk_is_read_only(self->super.qdisk))
    msg = _pop_head_qoverflow(self, path_options);

  if (!msg)
    {
      g_mutex_unlock(&s->lock);
      return NULL;
    }

success:
  if (!_maybe_move_messages_among_queue_segments(self))
    {
      stats_update = FALSE;
    }

  g_mutex_unlock(&s->lock);

  if (s->use_backlog)
    _push_tail_qbacklog(self, msg, path_options);

  if (stats_update)
    log_queue_queued_messages_dec(s);

  return msg;
}

static void
_push_head(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  g_assert_not_reached();
}

/* _is_msg_serialization_needed_hint() must be called without holding the queue's lock.
 * This can only be used _as a hint_ for performance considerations, because as soon as the lock
 * is released, there will be no guarantee that the result of this function remain correct. */
static inline gboolean
_is_msg_serialization_needed_hint(LogQueueDiskNonReliable *self)
{
  g_mutex_lock(&self->super.super.lock);

  gboolean msg_serialization_needed = FALSE;

  if (_can_push_to_qout(self))
    goto exit;

  if (self->qoverflow->length != 0)
    goto exit;

  if (!qdisk_started(self->super.qdisk) || !qdisk_is_space_avail(self->super.qdisk, 64))
    goto exit;

  msg_serialization_needed = TRUE;

exit:
  g_mutex_unlock(&self->super.super.lock);
  return msg_serialization_needed;
}

static gboolean
_ensure_serialized_and_write_to_disk(LogQueueDiskNonReliable *self, LogMessage *msg, GString *serialized_msg)
{
  if (serialized_msg)
    return qdisk_push_tail(self->super.qdisk, serialized_msg);

  return _serialize_and_write_message_to_disk(self, msg);
}

static inline void
_push_tail_qout(LogQueueDiskNonReliable *self, LogMessage *msg, const LogPathOptions *path_options)
{
  /* simple push never generates flow-control enabled entries to qout, they only get there
   * when rewinding the backlog */

  g_queue_push_tail(self->qout, msg);
  g_queue_push_tail(self->qout, LOG_PATH_OPTIONS_FOR_BACKLOG);

  log_queue_memory_usage_add(&self->super.super, log_msg_get_size(msg));

  log_msg_ack(msg, path_options, AT_PROCESSED);
}

static inline void
_push_tail_qoverflow(LogQueueDiskNonReliable *self, LogMessage *msg, const LogPathOptions *path_options)
{
  g_queue_push_tail(self->qoverflow, msg);
  g_queue_push_tail(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(path_options));

  log_queue_memory_usage_add(&self->super.super, log_msg_get_size(msg));

  /* no ack */
}

static inline gboolean
_push_tail_disk(LogQueueDiskNonReliable *self, LogMessage *msg, const LogPathOptions *path_options,
                GString *serialized_msg)
{
  if (!_ensure_serialized_and_write_to_disk(self, msg, serialized_msg))
    return FALSE;

  log_msg_ack(msg, path_options, AT_PROCESSED);
  log_msg_unref(msg);
  return TRUE;
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;

  ScratchBuffersMarker marker;
  GString *serialized_msg = NULL;

  if (_is_msg_serialization_needed_hint(self))
    {
      serialized_msg = scratch_buffers_alloc_and_mark(&marker);
      if (!log_queue_disk_serialize_msg(&self->super, msg, serialized_msg))
        {
          msg_error("Failed to serialize message for non-reliable disk-buffer, dropping message",
                    evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                    evt_tag_str("persist_name", s->persist_name));
          log_queue_disk_drop_message(&self->super, msg, path_options);
          scratch_buffers_reclaim_marked(marker);
          return;
        }
    }

  g_mutex_lock(&s->lock);

  /* we push messages into queue segments in the following order: qoverflow, disk, qout */
  if (_can_push_to_qout(self))
    {
      _push_tail_qout(self, msg, path_options);
      goto queued;
    }

  if (self->qoverflow->length != 0 || !_push_tail_disk(self, msg, path_options, serialized_msg))
    {
      if (HAS_SPACE_IN_QUEUE(self->qoverflow))
        {
          _push_tail_qoverflow(self, msg, path_options);
          goto queued;
        }

      msg_debug("Destination queue full, dropping message",
                evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                evt_tag_long("queue_len", log_queue_get_length(s)),
                evt_tag_int("mem_buf_length", self->qoverflow_size),
                evt_tag_long("disk_buf_size", qdisk_get_maximum_size(self->super.qdisk)),
                evt_tag_str("persist_name", s->persist_name));
      log_queue_disk_drop_message(&self->super, msg, path_options);
      goto exit;
    }

queued:
  log_queue_queued_messages_inc(s);

  /* this releases the queue's lock for a short time, which may violate the
   * consistency of the disk-buffer, so it must be the last call under lock in this function
   */
  log_queue_push_notify(s);

exit:
  g_mutex_unlock(&s->lock);
  if (serialized_msg)
    scratch_buffers_reclaim_marked(marker);
}

static void
_empty_queue(LogQueueDiskNonReliable *self, GQueue *q)
{
  while (q && !g_queue_is_empty(q))
    {
      LogMessage *lm;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      lm = g_queue_pop_head(q);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(q), &path_options);

      log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(lm));

      log_msg_ack(lm, &path_options, AT_PROCESSED);
      log_msg_unref(lm);
    }
}

static void
_free(LogQueue *s)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *)s;


  if (self->qout)
    {
      g_assert(g_queue_is_empty(self->qout));
      g_queue_free(self->qout);
      self->qout = NULL;
    }

  if (self->qbacklog)
    {
      g_assert(g_queue_is_empty(self->qbacklog));
      g_queue_free(self->qbacklog);
      self->qbacklog = NULL;
    }

  if (self->qoverflow)
    {
      g_assert(g_queue_is_empty(self->qoverflow));
      g_queue_free(self->qoverflow);
      self->qoverflow = NULL;
    }

  log_queue_disk_free_method(&self->super);
}

static gboolean
_stop(LogQueueDisk *s, gboolean *persistent)
{
  LogQueueDiskNonReliable *self = (LogQueueDiskNonReliable *) s;

  gboolean result = FALSE;

  if (qdisk_stop(s->qdisk, self->qout, self->qbacklog, self->qoverflow))
    {
      *persistent = TRUE;
      result = TRUE;
    }

  _empty_queue(self, self->qoverflow);
  _empty_queue(self, self->qout);
  _empty_queue(self, self->qbacklog);

  return result;
}

static gboolean
_stop_corrupted(LogQueueDisk *s)
{
  return qdisk_stop(s->qdisk, NULL, NULL, NULL);
}

static inline void
_set_logqueue_virtual_functions(LogQueue *s)
{
  s->get_length = _get_length;
  s->ack_backlog = _ack_backlog;
  s->rewind_backlog = _rewind_backlog;
  s->rewind_backlog_all = _rewind_backlog_all;
  s->pop_head = _pop_head;
  s->push_head = _push_head;
  s->push_tail = _push_tail;
  s->free_fn = _free;
}

static inline void
_set_logqueue_disk_virtual_functions(LogQueueDisk *s)
{
  s->start = _start;
  s->stop = _stop;
  s->stop_corrupted = _stop_corrupted;
}

static inline void
_set_virtual_functions(LogQueueDiskNonReliable *self)
{
  _set_logqueue_virtual_functions(&self->super.super);
  _set_logqueue_disk_virtual_functions(&self->super);
}

LogQueue *
log_queue_disk_non_reliable_new(DiskQueueOptions *options, const gchar *persist_name)
{
  g_assert(options->reliable == FALSE);
  LogQueueDiskNonReliable *self = g_new0(LogQueueDiskNonReliable, 1);
  log_queue_disk_init_instance(&self->super, options, "SLQF", persist_name);
  self->qbacklog = g_queue_new();
  self->qout = g_queue_new();
  self->qoverflow = g_queue_new();
  self->qout_size = options->qout_size;
  self->qoverflow_size = options->mem_buf_length;
  _set_virtual_functions(self);
  return &self->super.super;
}
