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
#include "mainloop.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iv_thread.h>

/*
 * LogFifo is a scalable first-in-first-output queue implementation, that:
 *
 *   - has a per-thread, unlocked input queue where threads can put their items
 *
 *   - has a locked wait-queue where items go once the per-thread input
 *     would be overflown or if the input thread goes to sleep (e.g.  one
 *     lock acquisition per a longer period)
 *
 *   - has an unlocked output queue where items from the wait queue go, once
 *     it becomes depleted.
 *
 * This means that items flow in this sequence from one list to the next:
 *
 *    input queue (per-thread) -> wait queue (locked) -> output queue (single-threaded)
 *
 * Fastpath is:
 *   - input threads putting elements on their per-thread queue (lockless)
 *   - output threads removing elements from the output queue (lockless)
 *
 * Slowpath:
 *   - input queue is overflown (or the input thread goes to sleep), wait
 *     queue mutex is grabbed, all elements are put to the wait queue.
 *
 *   - output queue is depleted, wait queue mutex is grabbed, all elements
 *     on the wait queue is put to the output queue
 *
 * Threading assumptions:
 *   - the head of the queue is only manipulated from the output thread
 *   - the tail of the queue is only manipulated from the input threads
 *
 */


typedef struct _LogQueueFifo
{
  LogQueue super;
  GStaticMutex lock;
  
  /* scalable qoverflow implementation */
  struct list_head qoverflow_output;
  struct list_head qoverflow_wait;
  gint qoverflow_wait_len;
  gint qoverflow_output_len;
  gint qoverflow_size; /* in number of elements */

  struct list_head qbacklog;    /* entries that were sent but not acked yet */
  gint qbacklog_len;

  GTimeVal last_throttle_check;
  gint parallel_push_notify_limit;
  LogQueuePushNotifyFunc parallel_push_notify;
  gpointer parallel_push_data;
  GDestroyNotify parallel_push_data_destroy;

  struct
  {
    struct list_head items;
    MainLoopIOWorkerFinishCallback cb;
    guint16 len;
    guint16 finish_cb_registered;
  } qoverflow_input[0];
} LogQueueFifo;

/* NOTE: this is inherently racy */
static gint64
log_queue_fifo_get_length(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  return self->qoverflow_wait_len + self->qoverflow_output_len;
}

/*
 * When this is called, it is assumed that the output thread is currently
 * not running (since this is the function that wakes it up), thus we can
 * access the length of the output queue without acquiring a lock.  Memory
 * barriers are covered by the use of the self->lock mutex, since
 * push_notify is registered under the protection of self->lock and after
 * that the length of the output queue will not change (since parallel_push
 * is only registered if it has less than enough items).
 *
 * NOTE: self->lock must have been acquired before calling this function.
 */
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

          g_static_mutex_unlock(&self->lock);

          func(user_data);
          if (destroy && user_data)
            destroy(user_data);

          g_static_mutex_lock(&self->lock);
        }
    }
}

static void
log_queue_fifo_move_input_unlocked(LogQueueFifo *self, gint thread_id)
{
  list_splice_tail_init(&self->qoverflow_input[thread_id].items, &self->qoverflow_wait);
  self->qoverflow_wait_len += self->qoverflow_input[thread_id].len;
  self->qoverflow_input[thread_id].len = 0;

  /* FIXME: handle queue overflows, probably needs an API rework,
   * log_queue_push_tail() currently returns FALSE to indicate that
   * we're unable to process the item, however the items on the input
   * lists were already accepted.  Probably the drop code should be
   * moved to this function, somehow.  */
}

static gpointer
log_queue_fifo_move_input(gpointer user_data)
{
  LogQueueFifo *self = (LogQueueFifo *) user_data;
  gint thread_id;

  thread_id = main_loop_io_worker_thread_id();

  g_assert(thread_id >= 0);

  g_static_mutex_lock(&self->lock);
  log_queue_fifo_move_input_unlocked(self, thread_id);
  log_queue_fifo_push_notify(self);
  g_static_mutex_unlock(&self->lock);
  self->qoverflow_input[thread_id].finish_cb_registered = FALSE;
  return NULL;
}



/**
 * Assumed to be called from one of the input threads. If the thread_id
 * cannot be determined, the item is put directly in the wait queue.
 *
 * Puts the message to the queue, and logs an error if it caused the
 * queue to be full.
 **/
static void
log_queue_fifo_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  gint thread_id;
  LogMessageQueueNode *node;

  thread_id = main_loop_io_worker_thread_id();

  g_assert(thread_id < 0 || log_queue_max_threads > thread_id);

  /* FIXME: hard-wired value */
  if (thread_id >= 0 && self->qoverflow_input[thread_id].len < 256)
    {
      /* fastpath, use per-thread input FIFOs */
      if (!self->qoverflow_input[thread_id].finish_cb_registered)
        {
          /* this is the first item in the input FIFO, register a finish
           * callback to make sure it gets moved to the wait_queue if the
           * input thread finishes */

          main_loop_io_worker_register_finish_callback(&self->qoverflow_input[thread_id].cb);
          self->qoverflow_input[thread_id].finish_cb_registered = TRUE;
        }

      node = log_msg_alloc_queue_node(msg, path_options);
      list_add_tail(&node->list, &self->qoverflow_input[thread_id].items);
      self->qoverflow_input[thread_id].len++;
      log_msg_unref(msg);
      return;
    }

  /* slow path, put the pending item and the whole input queue to the wait_queue */

  g_static_mutex_lock(&self->lock);
  
  if (thread_id >= 0)
    log_queue_fifo_move_input_unlocked(self, thread_id);
  
  node = log_msg_alloc_queue_node(msg, path_options);

  list_add_tail(&node->list, &self->qoverflow_wait);
  self->qoverflow_wait_len++;
  log_queue_fifo_push_notify(self);
  g_static_mutex_unlock(&self->lock);

  stats_counter_inc(self->super.stored_messages);
  log_msg_unref(msg);
  return;

#if 0
/* FIXME: no way to handle overflows now */

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
#endif
}

/*
 * This is assumed to be called only from the output thread, which means
 * that parallel_push_notify is NULL.
 */
static void
log_queue_fifo_push_head(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessageQueueNode *node;

  /* we don't check limits when putting items "in-front", as it
   * normally happens when we start processing an item, but at the end
   * can't deliver it. No checks, no drops either. */

  g_assert(self->parallel_push_notify == NULL);

  node = log_msg_alloc_dynamic_queue_node(msg, path_options);
  list_add(&node->list, &self->qoverflow_output);
  self->qoverflow_output_len++;

  stats_counter_inc(self->super.stored_messages);
}

static void
log_queue_fifo_reset_parallel_push(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  g_static_mutex_lock(&self->lock);
  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;
  self->parallel_push_notify_limit = 0;
  g_static_mutex_unlock(&self->lock);
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

  g_static_mutex_lock(&self->lock);

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
      g_static_mutex_unlock(&self->lock);
      return FALSE;
    }

  /* consume the user_data reference as we won't use the callback */
  if (user_data && user_data_destroy)
    user_data_destroy(user_data);

  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;

  g_static_mutex_unlock(&self->lock);
  
  /* recalculate buckets, throttle is only running in the output thread, no need to lock it. */

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
            }
          return FALSE;
        }
    }

  return TRUE;
}

/*
 * Can only run from the output thread.
 */
static gboolean
log_queue_fifo_pop_head(LogQueue *s, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessageQueueNode *node;

  g_assert(self->parallel_push_notify == NULL);

  if (self->super.throttle && self->super.throttle_buckets == 0)
    {
      return FALSE;
    }
    
  if (self->qoverflow_output_len == 0)
    {
      /* slow path, output queue is empty, get some elements from the wait queue */
      g_static_mutex_lock(&self->lock);
      list_splice_tail_init(&self->qoverflow_wait, &self->qoverflow_output);
      self->qoverflow_output_len = self->qoverflow_wait_len;
      self->qoverflow_wait_len = 0;
      g_static_mutex_unlock(&self->lock);
    }

  if (self->qoverflow_output_len > 0)
    {
      node = list_entry(self->qoverflow_output.next, LogMessageQueueNode, list);

      *msg = node->msg;
      path_options->flow_control = node->flow_controlled;
      self->qoverflow_output_len--;
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
      /* no items either on the wait queue nor the output queue.
       *
       * NOTE: the input queues may contain items even in this case,
       * however we don't touch them here, they'll be migrated to the
       * wait_queue once the input threads finish their processing (or
       * the high watermark is reached). Also, they are unlocked, so
       * no way to touch them safely.
       */
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
  return TRUE;
}

/*
 * Can only run from the output thread.
 */
static void
log_queue_fifo_ack_backlog(LogQueue *s, gint n)
{
  LogQueueFifo *self = (LogQueueFifo *) s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;

  g_assert(self->parallel_push_notify == NULL);

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
}


/*
 * log_queue_rewind_backlog:
 *
 * Move items on our backlog back to our qoverflow queue. Please note that this
 * function does not really care about qoverflow size, it has to put the backlog
 * somewhere. The backlog is emptied as that will be filled if we send the
 * items again.
 *
 * NOTE: this is assumed to be called from the output thread, e.g.
 * parallel_push must be disabled here.
 */
static void
log_queue_fifo_rewind_backlog(LogQueue *s)
{
  LogQueueFifo *self = (LogQueueFifo *) s;

  g_assert(self->parallel_push_notify == NULL);

  list_splice_tail_init(&self->qbacklog, &self->qoverflow_output);
  self->qoverflow_output_len += self->qbacklog_len;
  self->qbacklog_len = 0;
}

static void
log_queue_fifo_free_queue(struct list_head *q)
{
  while (!list_empty(q))
    {
      LogMessageQueueNode *node;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      node = list_entry(q->next, LogMessageQueueNode, list);
      list_del(&node->list);

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
  gint i;

  for (i = 0; i < log_queue_max_threads; i++)
    log_queue_fifo_free_queue(&self->qoverflow_input[i].items);

  log_queue_fifo_free_queue(&self->qoverflow_wait);
  log_queue_fifo_free_queue(&self->qoverflow_output);
  log_queue_fifo_free_queue(&self->qbacklog);
}

LogQueue *
log_queue_fifo_new(gint qoverflow_size, const gchar *persist_name)
{
  LogQueueFifo *self;
  gint i;

  self = g_malloc0(sizeof(LogQueueFifo) + log_queue_max_threads * sizeof(self->qoverflow_input[0]));

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
  
  for (i = 0; i < log_queue_max_threads; i++)
    {
      INIT_LIST_HEAD(&self->qoverflow_input[i].items);
      main_loop_io_worker_finish_callback_init(&self->qoverflow_input[i].cb);
      self->qoverflow_input[i].cb.user_data = self;
      self->qoverflow_input[i].cb.func = log_queue_fifo_move_input;
    }
  INIT_LIST_HEAD(&self->qoverflow_wait);
  INIT_LIST_HEAD(&self->qoverflow_output);
  INIT_LIST_HEAD(&self->qbacklog);

  self->qoverflow_size = qoverflow_size;
  g_static_mutex_init(&self->lock);
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
