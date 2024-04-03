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
#define PESSIMISTIC_FLOW_CONTROL_WINDOW_BYTES 10000 * 16 *1024
#define ENTRIES_PER_MSG_IN_MEM_Q 3

static inline void
_push_to_memory_queue_tail(GQueue *queue, gint64 position, LogMessage *msg, const LogPathOptions *path_options)
{
  gint64 *allocated_position = g_malloc(sizeof(gint64));
  *allocated_position = position;

  g_queue_push_tail(queue, allocated_position);
  g_queue_push_tail(queue, msg);
  g_queue_push_tail(queue, LOG_PATH_OPTIONS_TO_POINTER(path_options));
}

static inline void
_pop_from_memory_queue_head(GQueue *queue, gint64 *position, LogMessage **msg, LogPathOptions *path_options)
{
  gint64 *allocated_position = g_queue_pop_head(queue);

  *position = *allocated_position;
  g_free(allocated_position);

  *msg = g_queue_pop_head(queue);
  POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(queue), path_options);
}

static inline gint64
_peek_memory_queue_head_position(GQueue *queue)
{
  gint64 *position = g_queue_peek_head(queue);
  return *position;
}

static gboolean
_start(LogQueueDisk *s)
{
  return qdisk_start(s->qdisk, NULL, NULL, NULL);
}

static gboolean
_skip_message(LogQueueDisk *self)
{
  if (!qdisk_started(self->qdisk))
    return FALSE;

  return qdisk_remove_head(self->qdisk);
}

static void
_empty_queue(LogQueueDiskReliable *self, GQueue *queue)
{
  while (queue && queue->length > 0)
    {
      gint64 temppos;
      LogMessage *msg;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      _pop_from_memory_queue_head(queue, &temppos, &msg, &path_options);

      log_queue_memory_usage_sub(&self->super.super, log_msg_get_size(msg));
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

  g_mutex_lock(&s->lock);

  for (i = 0; i < num_msg_to_ack; i++)
    {
      if (qdisk_get_backlog_head (self->super.qdisk) == qdisk_get_next_head_position(self->super.qdisk))
        {
          goto exit_reliable;
        }
      if (self->backlog->length > 0)
        {
          if (_peek_memory_queue_head_position(self->backlog) == qdisk_get_backlog_head(self->super.qdisk))
            {
              gint64 position;
              _pop_from_memory_queue_head(self->backlog, &position, &msg, &path_options);

              log_queue_memory_usage_sub(s, log_msg_get_size(msg));
              log_msg_ack(msg, &path_options, AT_PROCESSED);
              log_msg_unref(msg);
            }
        }

      qdisk_ack_backlog(self->super.qdisk);
      log_queue_disk_update_disk_related_counters(&self->super);
    }
exit_reliable:
  qdisk_reset_file_if_empty(self->super.qdisk);
  g_mutex_unlock(&s->lock);
}

static gint
_find_pos_in_backlog(LogQueueDiskReliable *self, gint64 new_pos)
{
  gint result = -1;
  int i = 0;
  GList *item = self->backlog->tail;
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
_move_message_from_backlog_to_flow_control_window(LogQueueDiskReliable *self)
{
  gpointer ptr_opt = g_queue_pop_tail(self->backlog);
  gpointer ptr_msg = g_queue_pop_tail(self->backlog);
  gpointer ptr_pos = g_queue_pop_tail(self->backlog);

  g_queue_push_head(self->flow_control_window, ptr_opt);
  g_queue_push_head(self->flow_control_window, ptr_msg);
  g_queue_push_head(self->flow_control_window, ptr_pos);
}

static void
_rewind_from_backlog(LogQueueDiskReliable *self, gint64 new_pos)
{
  gint i;
  g_assert((self->backlog->length % 3) == 0);

  gint rewind_backlog_queue = _find_pos_in_backlog(self, new_pos);
  for (i = 0; i <= rewind_backlog_queue; i++)
    {
      _move_message_from_backlog_to_flow_control_window(self);
    }
}


static void
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  g_mutex_lock(&s->lock);

  rewind_count = MIN(rewind_count, qdisk_get_backlog_count(self->super.qdisk));
  qdisk_rewind_backlog(self->super.qdisk, rewind_count);
  _rewind_from_backlog(self, qdisk_get_next_head_position(self->super.qdisk));

  log_queue_queued_messages_add(s, rewind_count);

  g_mutex_unlock(&s->lock);
}

static void
_rewind_backlog_all(LogQueue *s)
{
  _rewind_backlog(s, G_MAXUINT);
}

static inline gboolean
_is_next_message_in_flow_control_window(LogQueueDiskReliable *self)
{
  if (self->flow_control_window->length == 0)
    return FALSE;

  return _peek_memory_queue_head_position(self->flow_control_window) == qdisk_get_next_head_position(self->super.qdisk);
}

static inline gboolean
_is_next_message_in_front_cache(LogQueueDiskReliable *self)
{
  if (self->front_cache->length == 0)
    return FALSE;

  return _peek_memory_queue_head_position(self->front_cache) == qdisk_get_next_head_position(self->super.qdisk);
}

static LogMessage *
_peek_head(LogQueue *s)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;
  LogMessage *msg = NULL;

  g_mutex_lock(&s->lock);

  if (_is_next_message_in_flow_control_window(self))
    {
      msg = g_queue_peek_nth(self->flow_control_window, 1);
      goto exit;
    }

  if (_is_next_message_in_front_cache(self))
    {
      msg = g_queue_peek_nth(self->front_cache, 1);
      goto exit;
    }

  msg = log_queue_disk_peek_message(&self->super);

exit:
  g_mutex_unlock(&s->lock);
  return msg;
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;
  LogMessage *msg = NULL;
  gboolean qdisk_corrupt = FALSE;

  g_mutex_lock(&s->lock);

  if (_is_next_message_in_flow_control_window(self))
    {
      gint64 position;
      _pop_from_memory_queue_head(self->flow_control_window, &position, &msg, path_options);
      log_queue_memory_usage_sub(s, log_msg_get_size(msg));

      if (!_skip_message(&self->super))
        qdisk_corrupt = TRUE;

      /* push to backlog */
      log_msg_ref(msg);
      _push_to_memory_queue_tail(self->backlog, position, msg, path_options);
      log_queue_memory_usage_add(s, log_msg_get_size(msg));

      goto exit;
    }

  if (_is_next_message_in_front_cache(self))
    {
      /*
       * Fast path: use the message from the memory, saving a disk read and a deserialization.
       */
      gint64 position;
      _pop_from_memory_queue_head(self->front_cache, &position, &msg, path_options);
      log_queue_memory_usage_sub(s, log_msg_get_size(msg));

      if (!_skip_message(&self->super))
        qdisk_corrupt = TRUE;

      goto exit;
    }

  msg = log_queue_disk_read_message(&self->super, path_options);

exit:
  if (!msg)
    {
      g_mutex_unlock(&s->lock);
      return NULL;
    }

  log_queue_disk_update_disk_related_counters(&self->super);
  log_queue_queued_messages_dec(s);

  if (qdisk_corrupt)
    log_queue_disk_restart_corrupted(&self->super);

  g_mutex_unlock(&s->lock);
  return msg;
}

static inline gboolean
_is_reserved_buffer_size_reached(LogQueueDiskReliable *self)
{
  return qdisk_get_empty_space(self->super.qdisk) < qdisk_get_flow_control_window_bytes(self->super.qdisk);
}

static inline gboolean
_is_space_available_in_front_cache(LogQueueDiskReliable *self)
{
  gint num_of_messages_in_front_cache = g_queue_get_length(self->front_cache) / ENTRIES_PER_MSG_IN_MEM_Q;
  return num_of_messages_in_front_cache < self->front_cache_size;
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;

  ScratchBuffersMarker marker;
  GString *serialized_msg = scratch_buffers_alloc_and_mark(&marker);
  if (!log_queue_disk_serialize_msg(&self->super, msg, serialized_msg))
    {
      msg_error("Failed to serialize message for reliable disk-buffer, dropping message",
                evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                evt_tag_str("persist_name", s->persist_name));
      log_queue_disk_drop_message(&self->super, msg, path_options);
      scratch_buffers_reclaim_marked(marker);
      return;
    }

  g_mutex_lock(&s->lock);

  gint64 message_position = qdisk_get_next_tail_position(self->super.qdisk);
  if (!qdisk_push_tail(self->super.qdisk, serialized_msg))
    {
      EVTTAG *suggestion = NULL;
      if (path_options->flow_control_requested)
        {
          suggestion = evt_tag_str("suggestion", "consider increasing flow-control-window-bytes() or decreasing "
                                   "log-iw-size() values on the source side to avoid message loss");
        }

      /* we were not able to store the msg, warn */
      msg_error("Destination reliable queue full, dropping message",
                evt_tag_str("filename", qdisk_get_filename(self->super.qdisk)),
                evt_tag_long("queue_len", log_queue_get_length(s)),
                evt_tag_int("flow_control_window_bytes", qdisk_get_flow_control_window_bytes(self->super.qdisk)),
                evt_tag_long("capacity_bytes", qdisk_get_maximum_size(self->super.qdisk)),
                evt_tag_str("persist_name", s->persist_name),
                suggestion);

      log_queue_disk_drop_message(&self->super, msg, path_options);
      scratch_buffers_reclaim_marked(marker);
      g_mutex_unlock(&s->lock);
      return;
    }

  log_queue_disk_update_disk_related_counters(&self->super);

  scratch_buffers_reclaim_marked(marker);

  if (_is_reserved_buffer_size_reached(self))
    {
      /*
       * Keep the message in memory, and do not ack it, so flow-control can kick in.
       */
      _push_to_memory_queue_tail(self->flow_control_window, message_position, msg, path_options);
      log_queue_memory_usage_add(s, log_msg_get_size(msg));
      goto exit;
    }

  log_msg_ack(msg, path_options, AT_PROCESSED);

  if (_is_space_available_in_front_cache(self))
    {
      /*
       * Keep the message in memory for fast-path.
       * Set its ack_needed to FALSE, because we have already acked it.
       */
      LogPathOptions local_path_options;
      log_path_options_chain(&local_path_options, path_options);
      local_path_options.ack_needed = FALSE;
      _push_to_memory_queue_tail(self->front_cache, message_position, msg, &local_path_options);
      log_queue_memory_usage_add(s, log_msg_get_size(msg));
      goto exit;
    }

  log_msg_unref(msg);

exit:
  log_queue_queued_messages_inc(s);

  /* this releases the queue's lock for a short time, which may violate the
   * consistency of the disk-buffer, so it must be the last call under lock in this function
   */
  log_queue_push_notify(s);
  g_mutex_unlock(&s->lock);
}

static void
_free(LogQueue *s)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *)s;


  if (self->flow_control_window)
    {
      g_assert(g_queue_is_empty(self->flow_control_window));
      g_queue_free(self->flow_control_window);
      self->flow_control_window = NULL;
    }

  if (self->backlog)
    {
      g_assert(g_queue_is_empty(self->backlog));
      g_queue_free(self->backlog);
      self->backlog = NULL;
    }

  if (self->front_cache)
    {
      g_assert(g_queue_is_empty(self->front_cache));
      g_queue_free(self->front_cache);
      self->front_cache = NULL;
    }

  log_queue_disk_free_method(&self->super);
}

static gboolean
_stop(LogQueueDisk *s, gboolean *persistent)
{
  LogQueueDiskReliable *self = (LogQueueDiskReliable *) s;

  gboolean result = FALSE;

  if (qdisk_stop(s->qdisk, NULL, NULL, NULL))
    {
      *persistent = TRUE;
      result = TRUE;
    }

  _empty_queue(self, self->flow_control_window);
  _empty_queue(self, self->front_cache);
  _empty_queue(self, self->backlog);

  return result;
}

static inline void
_set_logqueue_virtual_functions(LogQueue *s)
{
  s->get_length = _get_length;
  s->ack_backlog = _ack_backlog;
  s->rewind_backlog = _rewind_backlog;
  s->rewind_backlog_all = _rewind_backlog_all;
  s->pop_head = _pop_head;
  s->peek_head = _peek_head;
  s->push_tail = _push_tail;
  s->free_fn = _free;
}

static inline void
_set_logqueue_disk_virtual_functions(LogQueueDisk *s)
{
  s->start = _start;
  s->stop = _stop;
}

static inline void
_set_virtual_functions(LogQueueDiskReliable *self)
{
  _set_logqueue_virtual_functions(&self->super.super);
  _set_logqueue_disk_virtual_functions(&self->super);
}

LogQueue *
log_queue_disk_reliable_new(DiskQueueOptions *options, const gchar *filename, const gchar *persist_name,
                            gint stats_level, StatsClusterKeyBuilder *driver_sck_builder,
                            StatsClusterKeyBuilder *queue_sck_builder)
{
  g_assert(options->reliable == TRUE);
  LogQueueDiskReliable *self = g_new0(LogQueueDiskReliable, 1);
  log_queue_disk_init_instance(&self->super, options, "SLRQ", filename, persist_name, stats_level,
                               driver_sck_builder, queue_sck_builder);
  if (options->flow_control_window_bytes < 0)
    {
      options->flow_control_window_bytes = PESSIMISTIC_FLOW_CONTROL_WINDOW_BYTES;
    }
  self->flow_control_window = g_queue_new();
  self->backlog = g_queue_new();
  self->front_cache = g_queue_new();
  self->front_cache_size = options->front_cache_size;
  _set_virtual_functions(self);
  return &self->super.super;
}
