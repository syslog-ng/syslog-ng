/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "logqueue.h"
#include "logpipe.h"
#include "messages.h"
#include "serialize.h"
#include "stats.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


typedef struct _LogQueueFifo
{
  LogQueue super;
  GMutex *lock;
  struct list_head qoverflow;   /* entries that did not fit to the disk based queue */
  gint qoverflow_len;
  struct list_head qbacklog;    /* entries that were sent but not acked yet */
  gint qbacklog_len;

  gint qoverflow_size; /* in number of elements */
  GTimeVal last_throttle_check;
  gint parallel_push_notify_limit;
  LogQueuePushNotifyFunc parallel_push_notify;
  gpointer parallel_push_data;
  GDestroyNotify parallel_push_data_destroy;
} LogQueueFifo;

/* NOTE: this is inherently racy */
static gint64
log_queue_fifo_get_length(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  return self->qoverflow_len;
}

static void
log_queue_fifo_push_notify(LogQueueFifo *self)
{
  if (self->parallel_push_notify)
    {
      gint64 num_elements;

      num_elements = log_queue_get_length(&self->super);
      if (num_elements >= self->parallel_push_notify_limit)
        {
          /* make sure the callback can call log_queue_check_items() again */
          GDestroyNotify destroy = self->parallel_push_data_destroy;
          gpointer user_data = self->parallel_push_data;
          LogQueuePushNotifyFunc func = self->parallel_push_notify;

          self->parallel_push_data = NULL;
          self->parallel_push_data_destroy = NULL;
          self->parallel_push_notify = NULL;

          g_mutex_unlock(self->lock);

          func(user_data);
          if (destroy && user_data)
            destroy(user_data);

          g_mutex_lock(self->lock);
        }
    }
}

/**
 * Puts the message to the queue, and logs an error if it caused the
 * queue to be full. The message is not dropped here though.
 **/
static void
log_queue_fifo_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogPathOptions local_options = *path_options;

  g_mutex_lock(self->lock);
  if (self->qoverflow_len < self->qoverflow_size)
    {
      LogMessageQueueNode *node;

      node = log_msg_alloc_queue_node(msg, path_options);
      list_add_tail(&node->list, &self->qoverflow);
      self->qoverflow_len++;

      local_options.flow_control = FALSE;
      log_queue_fifo_push_notify(self);
      g_mutex_unlock(self->lock);
    }
  else
    {
      g_mutex_unlock(self->lock);

      stats_counter_inc(self->super.dropped_messages);
      log_msg_drop(msg, path_options);

      msg_debug("Destination queue full, dropping message",
                evt_tag_int("queue_len", log_queue_get_length(s)),
                evt_tag_int("log_fifo_size", self->qoverflow_size),
                NULL);
      return;
    }
  stats_counter_inc(self->super.stored_messages);
  log_msg_ack(msg, &local_options);
  log_msg_unref(msg);
}

static void
log_queue_fifo_push_head(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessageQueueNode *node;

  /* we don't check limits when putting items "in-front", as it
   * normally happens when we start processing an item, but at the end
   * can't deliver it. No checks, no drops either. */

  g_mutex_lock(self->lock);
  node = log_msg_alloc_dynamic_queue_node(msg, path_options);
  list_add(&node->list, &self->qoverflow);
  self->qoverflow_len++;

  log_queue_fifo_push_notify(self);
  g_mutex_unlock(self->lock);
  stats_counter_inc(self->super.stored_messages);
}

static void
log_queue_fifo_reset_parallel_push(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  g_mutex_lock(self->lock);
  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;
  self->parallel_push_notify_limit = 0;
  g_mutex_unlock(self->lock);
}

/*
 *
 * @batch_items: the number of items processed in a batch (e.g. the number of items the consumer is preferred to process at a single invocation)
 * @partial_batch: true is returned if some elements (but less than batch_items) are already buffered
 * @timeout: the number of milliseconds that the consumer needs to wait before we can possibly proceed
 */
static gboolean
log_queue_fifo_check_items(LogQueue *s, gint batch_items, gboolean *partial_batch, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  gint64 num_elements;

  g_mutex_lock(self->lock);

  /* drop reference to the previous callback/userdata */
  if (self->parallel_push_data && self->parallel_push_data_destroy)
    self->parallel_push_data_destroy(self->parallel_push_data);

  num_elements = log_queue_get_length(&self->super);
  if (num_elements == 0 || num_elements < batch_items)
    {
      self->parallel_push_notify = parallel_push_notify;
      self->parallel_push_data = user_data;
      self->parallel_push_data_destroy = user_data_destroy;
      if (num_elements == 0)
        {
          /* NOTE: special case, 0->1 transition must be communicated,
           * partial_batch is FALSE in this case.
           *
           * we need to tell the caller the first item as it arrives on the
           * queue, as it needs to arm its flush_timeout timer in this case,
           * which is unarmed as long as 0 elements are available, but then
           * the first item wouldn't be flushed after flush_timeout.
           * */
          self->parallel_push_notify_limit = 1;
          if (partial_batch)
            *partial_batch = FALSE;
        }
      else
        {
          if (partial_batch)
            *partial_batch = TRUE;
          self->parallel_push_notify_limit = batch_items;
        }
      g_mutex_unlock(self->lock);
      return FALSE;
    }

  /* consume the user_data reference as we won't use the callback */
  if (user_data && user_data_destroy)
    user_data_destroy(user_data);

  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;

  /* recalculate buckets */

  if (self->super.throttle > 0)
    {
      gint64 diff;
      gint new_buckets;
      GTimeVal now;

      g_get_current_time(&now);
      /* throttling is enabled, calculate new buckets */
      if (self->last_throttle_check.tv_sec != 0)
        {
          diff = g_time_val_diff(&now, &self->last_throttle_check);
        }
      else
        {
          diff = 0;
          self->last_throttle_check = now;
        }
      new_buckets = (self->super.throttle * diff) / G_USEC_PER_SEC;
      if (new_buckets)
        {

          /* if new_buckets is zero, we don't save the current time as
           * last_throttle_check. The reason is that new_buckets could be
           * rounded to zero when only a minimal interval passes between
           * poll iterations.
           */
          self->super.throttle_buckets = MIN(self->super.throttle, self->super.throttle_buckets + new_buckets);
          self->last_throttle_check = now;
        }
      if (num_elements && self->super.throttle_buckets == 0)
        {
          if (timeout)
            {
              /* we are unable to send because of throttling, make sure that we
               * wake up when the rate limits lets us send at least 1 message */
              *timeout = (1000 / self->super.throttle) + 1;
              msg_debug("Throttling output",
                        evt_tag_int("wait", *timeout),
                        NULL);
              g_mutex_unlock(self->lock);
            }
          return FALSE;
        }
    }

  g_mutex_unlock(self->lock);
  return TRUE;
}

static gboolean
log_queue_fifo_pop_head(LogQueue *s, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessageQueueNode *node;

  g_mutex_lock(self->lock);
  if (self->super.throttle && self->super.throttle_buckets == 0)
    {
      g_mutex_unlock(self->lock);
      return FALSE;
    }
  if (self->qoverflow_len > 0)
    {
      node = list_entry(self->qoverflow.next, LogMessageQueueNode, list);
      *msg = node->msg;
      path_options->flow_control = node->flow_controlled;
      self->qoverflow_len--;
      if (!push_to_backlog)
        {
          list_del(&node->list);
          log_msg_free_queue_node(node);
        }
      else
        {
          list_del_init(&node->list);
        }
    }
  else
    {
      g_mutex_unlock(self->lock);
      return FALSE;
    }
  stats_counter_dec(self->super.stored_messages);

  if (push_to_backlog)
    {
      log_msg_ref(*msg);
      list_add_tail(&node->list, &self->qbacklog);
      self->qbacklog_len++;
    }
  self->super.throttle_buckets--;
  g_mutex_unlock(self->lock);
  return TRUE;
}

static void
log_queue_fifo_ack_backlog(LogQueue *s, gint n)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;

  g_mutex_lock(self->lock);
  for (i = 0; i < n && self->qbacklog_len > 0; i++)
    {
      LogMessageQueueNode *node;

      node = list_entry(self->qbacklog.next, LogMessageQueueNode, list);
      msg = node->msg;
      path_options.flow_control = node->flow_controlled;

      list_del(&node->list);
      log_msg_free_queue_node(node);
      self->qbacklog_len--;

      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }
  g_mutex_unlock(self->lock);
}


/* Move items on our backlog back to our qoverflow queue. Please note that this
 * function does not really care about qoverflow size, it has to put the backlog
 * somewhere. The backlog is emptied as that will be filled if we send the
 * items again.
 *
 */
static void
log_queue_fifo_rewind_backlog(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  gint i, n;

  g_mutex_lock(self->lock);
  n = self->qbacklog_len;
  for (i = 0; i < n; i++)
    {
      LogMessageQueueNode *node;

      node = list_entry(self->qbacklog.next, LogMessageQueueNode, list);
      list_add(&node->list, &self->qoverflow);
    }
  log_queue_fifo_push_notify(self);
  g_mutex_unlock(self->lock);
}

static void
log_queue_fifo_free_queue(struct list_head *q)
{
  while (!list_empty(q))
    {
      LogMessageQueueNode *node;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      node = list_entry(q->next, LogMessageQueueNode, list);
      path_options.flow_control = node->flow_controlled;
      log_msg_ack(node->msg, &path_options);
      log_msg_unref(node->msg);
      log_msg_free_queue_node(node);
    }
}

static void
log_queue_fifo_free(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  log_queue_fifo_free_queue(&self->qoverflow);
  log_queue_fifo_free_queue(&self->qbacklog);
  g_mutex_free(self->lock);
}

LogQueue *
log_queue_fifo_new(gint qoverflow_size, const gchar *persist_name)
{
  LogQueueFifo *self = g_new0(LogQueueFifo, 1);

  log_queue_init_instance(&self->super, persist_name);
  self->super.get_length = log_queue_fifo_get_length;
  self->super.push_tail = log_queue_fifo_push_tail;
  self->super.push_head = log_queue_fifo_push_head;
  self->super.pop_head = log_queue_fifo_pop_head;
  self->super.ack_backlog = log_queue_fifo_ack_backlog;
  self->super.rewind_backlog = log_queue_fifo_rewind_backlog;

  self->super.reset_parallel_push = log_queue_fifo_reset_parallel_push;
  self->super.check_items = log_queue_fifo_check_items;

  self->super.free_fn = log_queue_fifo_free;

  INIT_LIST_HEAD(&self->qoverflow);
  INIT_LIST_HEAD(&self->qbacklog);

  self->qoverflow_size = qoverflow_size;
  self->lock = g_mutex_new();
  return &self->super;
}

#if 0
void
log_queue_drop_message_warning(LogQueue *self, gchar *property)
{
  if (self->super.drop_message_warning)
    /* call the 'inherited' method */
    self->super.drop_message_warning(&self->super, property);
  else {
    char msg[128];
    if (!property)
      property = "";
    snprintf(msg, 128, "Destination queue full, dropping %smessage", property);
    msg_debug(msg,
           evt_tag_int("queue_len", log_queue_get_length(self)),
           evt_tag_int("log_fifo_size", self->qoverflow_size),
           NULL);
    }
}

gboolean
log_queue_save_queue(LogQueue *self, gboolean *persistent)
{
  if (self->super.save_queue)
    /* call the 'inherited' method */
    return self->super.save_queue(&self->super, persistent);

  /* default behaviour: save the world  - do nothing */
  *persistent = TRUE;
  return TRUE;
}
#endif
