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


#define LOG_PATH_OPTIONS_TO_POINTER(lpo) GUINT_TO_POINTER(0x80000000 | (lpo)->flow_control)

/* NOTE: this must not evaluate ptr multiple times, otherwise the code that
 * uses this breaks, as it passes the result of a g_queue_pop_head call,
 * which has side effects.
 */
#define POINTER_TO_LOG_PATH_OPTIONS(ptr, lpo) (lpo)->flow_control = (GPOINTER_TO_INT(ptr) & ~0x80000000)

struct _LogQueue
{
  GMutex *lock;
  GQueue *qoverflow;   /* entries that did not fit to the disk based queue */
  GQueue *qbacklog;    /* entries that were sent but not acked yet */
  gint qoverflow_size; /* in number of elements */
  guint32 *stored_messages;
  gint throttle;
  gint throttle_buckets;
  GTimeVal last_throttle_check;
  gint parallel_push_notify_limit;
  LogQueuePushNotifyFunc parallel_push_notify;
  gpointer parallel_push_data;
  GDestroyNotify parallel_push_data_destroy;
};

/* NOTE: this is inherently racy */
gint64 
log_queue_get_length(LogQueue *self)
{
  return (self->qoverflow->length / 2);
}

static void
log_queue_push_notify(LogQueue *self)
{
  if (self->parallel_push_notify)
    {
      gint64 num_elements;

      num_elements = log_queue_get_length(self);
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
 *
 * Returns: TRUE if the messages could be consumed, FALSE otherwise
 **/
gboolean
log_queue_push_tail(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  LogPathOptions local_options = *path_options;
  
  g_mutex_lock(self->lock);
  if ((self->qoverflow->length / 2) < self->qoverflow_size)
    {
      g_queue_push_tail(self->qoverflow, msg);
      g_queue_push_tail(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(path_options));
      msg->flags |= LF_STATE_REFERENCED;
      log_msg_ref(msg);
      local_options.flow_control = FALSE;
      log_queue_push_notify(self);
      g_mutex_unlock(self->lock);
    }
  else
    {
      g_mutex_unlock(self->lock);
      return FALSE;
    }
  stats_counter_inc(self->stored_messages);
  log_msg_ack(msg, &local_options);
  log_msg_unref(msg);
  return TRUE;
}

gboolean
log_queue_push_head(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  g_mutex_lock(self->lock);
  g_queue_push_head(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(path_options));
  g_queue_push_head(self->qoverflow, msg);
  log_queue_push_notify(self);
  g_mutex_unlock(self->lock);
  msg->flags |= LF_STATE_REFERENCED;
  stats_counter_inc(self->stored_messages);
  return TRUE;
}

void
log_queue_reset_parallel_push(LogQueue *self)
{
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
gboolean
log_queue_check_items(LogQueue *self, gint batch_items, gboolean *partial_batch, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy)
{
  gint64 num_elements;

  g_mutex_lock(self->lock);

  /* drop reference to the previous callback/userdata */
  if (self->parallel_push_data && self->parallel_push_data_destroy)
    self->parallel_push_data_destroy(self->parallel_push_data);

  num_elements = log_queue_get_length(self);
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

  if (self->throttle > 0)
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
      new_buckets = (self->throttle * diff) / G_USEC_PER_SEC;
      if (new_buckets)
        {

          /* if new_buckets is zero, we don't save the current time as
           * last_throttle_check. The reason is that new_buckets could be
           * rounded to zero when only a minimal interval passes between
           * poll iterations.
           */
          self->throttle_buckets = MIN(self->throttle, self->throttle_buckets + new_buckets);
          self->last_throttle_check = now;
        }
      if (num_elements && self->throttle_buckets == 0)
        {
          if (timeout)
            {
              /* we are unable to send because of throttling, make sure that we
               * wake up when the rate limits lets us send at least 1 message */
              *timeout = (1000 / self->throttle) + 1;
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

gboolean
log_queue_pop_head(LogQueue *self, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog)
{
  g_mutex_lock(self->lock);
  if (self->throttle && self->throttle_buckets == 0)
    {
      g_mutex_unlock(self->lock);
      return FALSE;
    }
  if (self->qoverflow->length > 0)
    {
      *msg = g_queue_pop_head(self->qoverflow);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qoverflow), path_options);
    }
  else
    {
      g_mutex_unlock(self->lock);
      return FALSE;
    }
  stats_counter_dec(self->stored_messages);

  if (push_to_backlog)
    {
      log_msg_ref(*msg);
      g_queue_push_tail(self->qbacklog, *msg);
      g_queue_push_tail(self->qbacklog, LOG_PATH_OPTIONS_TO_POINTER(path_options));
    }
  self->throttle_buckets--;
  g_mutex_unlock(self->lock);
  return TRUE;
}

void
log_queue_ack_backlog(LogQueue *self, gint n)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;
  
  g_mutex_lock(self->lock);
  for (i = 0; i < n; i++)
    {
      msg = g_queue_pop_head(self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
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
void
log_queue_rewind_backlog(LogQueue *self)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i, n;

  g_mutex_lock(self->lock);
  n = self->qbacklog->length / 2;
  for (i = 0; i < n; i++)
    {
      msg = g_queue_pop_head(self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
      
      /* NOTE: reverse order as we are pushing to the head */
      g_queue_push_head(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(&path_options));
      g_queue_push_head(self->qoverflow, msg);
    }
  log_queue_push_notify(self);
  g_mutex_unlock(self->lock);
}

void
log_queue_set_throttle(LogQueue *self, gint throttle)
{
  self->throttle = throttle;
  self->throttle_buckets = throttle;
}

LogQueue *
log_queue_new(gint qoverflow_size)
{
  LogQueue *self = g_new0(LogQueue, 1);
  
  self->qoverflow = g_queue_new();
  self->qbacklog = g_queue_new();
  self->qoverflow_size = qoverflow_size;
  self->lock = g_mutex_new();
  return self;
}

static void
log_queue_free_queue(GQueue *q)
{
  while (!g_queue_is_empty(q))
    {
      LogMessage *lm;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      
      lm = g_queue_pop_head(q);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(q), &path_options);
      log_msg_ack(lm, &path_options);
      log_msg_unref(lm);
    }
  g_queue_free(q);
}

void
log_queue_free(LogQueue *self)
{
  log_queue_free_queue(self->qoverflow);
  log_queue_free_queue(self->qbacklog);
  g_mutex_free(self->lock);
  g_free(self);
}


