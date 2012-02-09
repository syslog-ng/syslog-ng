#include "logqueue.h"
#include "stats.h"
#include "messages.h"

gint log_queue_max_threads = 0;

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
void
log_queue_push_notify(LogQueue *self)
{
  if (self->parallel_push_notify)
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

void
log_queue_reset_parallel_push(LogQueue *self)
{
  g_static_mutex_lock(&self->lock);
  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;
  g_static_mutex_unlock(&self->lock);
}

void
log_queue_set_parallel_push(LogQueue *self, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy)
{
  g_static_mutex_lock(&self->lock);
  self->parallel_push_notify = parallel_push_notify;
  self->parallel_push_data = user_data;
  self->parallel_push_data_destroy = user_data_destroy;
  g_static_mutex_unlock(&self->lock);
}

/*
 *
 * @batch_items: the number of items processed in a batch (e.g. the number of items the consumer is preferred to process at a single invocation)
 * @partial_batch: true is returned if some elements (but less than batch_items) are already buffered
 * @timeout: the number of milliseconds that the consumer needs to wait before we can possibly proceed
 */
gboolean
log_queue_check_items(LogQueue *self, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy)
{
  gint64 num_elements;

  g_static_mutex_lock(&self->lock);

  /* drop reference to the previous callback/userdata */
  if (self->parallel_push_data && self->parallel_push_data_destroy)
    self->parallel_push_data_destroy(self->parallel_push_data);

  num_elements = log_queue_get_length(self);
  if (num_elements == 0)
    {
      self->parallel_push_notify = parallel_push_notify;
      self->parallel_push_data = user_data;
      self->parallel_push_data_destroy = user_data_destroy;
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
            }
          return FALSE;
        }
    }

  return TRUE;
}

void
log_queue_set_counters(LogQueue *self, StatsCounterItem *stored_messages, StatsCounterItem *dropped_messages)
{
  self->stored_messages = stored_messages;
  self->dropped_messages = dropped_messages;
  stats_counter_set(self->stored_messages, log_queue_get_length(self));
}

void
log_queue_init_instance(LogQueue *self, const gchar *persist_name)
{
  self->ref_cnt = 1;
  self->free_fn = log_queue_free_method;

  self->persist_name = persist_name ? g_strdup(persist_name) : NULL;
  g_static_mutex_init(&self->lock);
}

void
log_queue_free_method(LogQueue *self)
{
  g_free(self->persist_name);
  g_free(self);
}

void
log_queue_set_max_threads(gint max_threads)
{
  log_queue_max_threads = max_threads;
}
