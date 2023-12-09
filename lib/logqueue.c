/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "messages.h"
#include "timeutils/misc.h"

void
log_queue_memory_usage_add(LogQueue *self, gsize value)
{
  stats_counter_add(self->metrics.shared.memory_usage, value);
  stats_counter_add(self->metrics.owned.memory_usage, value);
}

void
log_queue_memory_usage_sub(LogQueue *self, gsize value)
{
  stats_counter_sub(self->metrics.shared.memory_usage, value);
  stats_counter_sub(self->metrics.owned.memory_usage, value);
}

void
log_queue_queued_messages_add(LogQueue *self, gsize value)
{
  stats_counter_add(self->metrics.shared.queued_messages, value);
  stats_counter_add(self->metrics.owned.queued_messages, value);
}

void
log_queue_queued_messages_sub(LogQueue *self, gsize value)
{
  stats_counter_sub(self->metrics.shared.queued_messages, value);
  stats_counter_sub(self->metrics.owned.queued_messages, value);
}

void
log_queue_queued_messages_inc(LogQueue *self)
{
  stats_counter_inc(self->metrics.shared.queued_messages);
  stats_counter_inc(self->metrics.owned.queued_messages);
}

void
log_queue_queued_messages_dec(LogQueue *self)
{
  stats_counter_dec(self->metrics.shared.queued_messages);
  stats_counter_dec(self->metrics.owned.queued_messages);
}

void
log_queue_queued_messages_reset(LogQueue *self)
{
  stats_counter_sub(self->metrics.shared.queued_messages,
                    stats_counter_get(self->metrics.owned.queued_messages));

  stats_counter_set(self->metrics.owned.queued_messages, log_queue_get_length(self));
  stats_counter_add(self->metrics.shared.queued_messages,
                    stats_counter_get(self->metrics.owned.queued_messages));
}

void
log_queue_dropped_messages_inc(LogQueue *self)
{
  stats_counter_inc(self->metrics.shared.dropped_messages);
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

      g_mutex_unlock(&self->lock);

      func(user_data);
      if (destroy && user_data)
        destroy(user_data);

      g_mutex_lock(&self->lock);
    }
}

void
log_queue_reset_parallel_push(LogQueue *self)
{
  g_mutex_lock(&self->lock);
  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;
  g_mutex_unlock(&self->lock);
}

void
log_queue_set_parallel_push(LogQueue *self, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data,
                            GDestroyNotify user_data_destroy)
{
  g_mutex_lock(&self->lock);
  self->parallel_push_notify = parallel_push_notify;
  self->parallel_push_data = user_data;
  self->parallel_push_data_destroy = user_data_destroy;
  g_mutex_unlock(&self->lock);
}

/*
 *
 * @batch_items: the number of items processed in a batch (e.g. the number of items the consumer is preferred to process at a single invocation)
 * @partial_batch: true is returned if some elements (but less than batch_items) are already buffered
 * @timeout: the number of milliseconds that the consumer needs to wait before we can possibly proceed
 */
gboolean
log_queue_check_items(LogQueue *self, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data,
                      GDestroyNotify user_data_destroy)
{
  gint64 num_elements;

  g_mutex_lock(&self->lock);

  /* drop reference to the previous callback/userdata */
  if (self->parallel_push_data && self->parallel_push_data_destroy)
    self->parallel_push_data_destroy(self->parallel_push_data);

  num_elements = log_queue_get_length(self);
  if (num_elements == 0)
    {
      self->parallel_push_notify = parallel_push_notify;
      self->parallel_push_data = user_data;
      self->parallel_push_data_destroy = user_data_destroy;
      g_mutex_unlock(&self->lock);
      return FALSE;
    }

  /* consume the user_data reference as we won't use the callback */
  if (user_data && user_data_destroy)
    user_data_destroy(user_data);

  self->parallel_push_notify = NULL;
  self->parallel_push_data = NULL;

  g_mutex_unlock(&self->lock);

  /* recalculate buckets, throttle is only running in the output thread, no need to lock it. */

  if (self->throttle > 0)
    {
      gint64 diff;
      gint new_buckets;
      struct timespec now;

      clock_gettime(CLOCK_MONOTONIC, &now);
      /* throttling is enabled, calculate new buckets */
      if (self->last_throttle_check.tv_sec != 0)
        {
          diff = timespec_diff_nsec(&now, &self->last_throttle_check);
        }
      else
        {
          diff = 0;
          self->last_throttle_check = now;
        }
      new_buckets = (self->throttle * diff) / NSEC_PER_SEC;
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
                        evt_tag_int("wait", *timeout));
            }
          return FALSE;
        }
    }

  return TRUE;
}

static void
_register_shared_counters(LogQueue *self, gint stats_level, StatsClusterKeyBuilder *builder)
{
  if (!builder)
    return;

  stats_cluster_key_builder_push(builder);
  {
    stats_cluster_key_builder_set_name(builder, "output_events_total");
    self->metrics.shared.output_events_sc_key = stats_cluster_key_builder_build_logpipe(builder);
  }
  stats_cluster_key_builder_pop(builder);

  stats_cluster_key_builder_push(builder);
  {
    stats_cluster_key_builder_set_legacy_alias(builder,
                                               self->metrics.shared.output_events_sc_key->legacy.component,
                                               self->metrics.shared.output_events_sc_key->legacy.id,
                                               self->metrics.shared.output_events_sc_key->legacy.instance);
    stats_cluster_key_builder_set_legacy_alias_name(builder, "memory_usage");
    self->metrics.shared.memory_usage_sc_key = stats_cluster_key_builder_build_single(builder);
  }
  stats_cluster_key_builder_pop(builder);

  stats_lock();
  {
    stats_register_counter(stats_level, self->metrics.shared.output_events_sc_key, SC_TYPE_QUEUED,
                           &self->metrics.shared.queued_messages);
    stats_register_counter(stats_level, self->metrics.shared.output_events_sc_key, SC_TYPE_DROPPED,
                           &self->metrics.shared.dropped_messages);
    stats_register_counter(stats_level, self->metrics.shared.memory_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.shared.memory_usage);
  }
  stats_unlock();
}

static void
_register_owned_counters(LogQueue *self, gint stats_level, StatsClusterKeyBuilder *builder)
{
  if (!builder)
    return;

  stats_cluster_key_builder_push(builder);
  {
    stats_cluster_key_builder_set_name(builder, "events");
    self->metrics.owned.events_sc_key = stats_cluster_key_builder_build_single(builder);

    stats_cluster_key_builder_set_name(builder, "memory_usage_bytes");
    self->metrics.owned.memory_usage_sc_key = stats_cluster_key_builder_build_single(builder);
  }
  stats_cluster_key_builder_pop(builder);

  stats_lock();
  {
    stats_register_counter(stats_level, self->metrics.owned.events_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.owned.queued_messages);
    stats_register_counter(stats_level, self->metrics.owned.memory_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                           &self->metrics.owned.memory_usage);
  }
  stats_unlock();
}

static void
_register_counters(LogQueue *self, gint stats_level, StatsClusterKeyBuilder *driver_sck_builder,
                   StatsClusterKeyBuilder *queue_sck_builder)
{
  g_assert(!driver_sck_builder || queue_sck_builder);

  _register_shared_counters(self, stats_level, driver_sck_builder);
  _register_owned_counters(self, stats_level, queue_sck_builder);
}

static void
_unregister_shared_counters(LogQueue *self)
{
  stats_lock();
  {
    if (self->metrics.shared.output_events_sc_key)
      {
        log_queue_queued_messages_sub(self, stats_counter_get(self->metrics.owned.queued_messages));
        stats_unregister_counter(self->metrics.shared.output_events_sc_key, SC_TYPE_QUEUED,
                                 &self->metrics.shared.queued_messages);
        stats_unregister_counter(self->metrics.shared.output_events_sc_key, SC_TYPE_DROPPED,
                                 &self->metrics.shared.dropped_messages);

        stats_cluster_key_free(self->metrics.shared.output_events_sc_key);
      }

    if (self->metrics.shared.memory_usage_sc_key)
      {
        log_queue_memory_usage_sub(self, stats_counter_get(self->metrics.owned.memory_usage));
        stats_unregister_counter(self->metrics.shared.memory_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.shared.memory_usage);

        stats_cluster_key_free(self->metrics.shared.memory_usage_sc_key);
      }
  }
  stats_unlock();
}

static void
_unregister_owned_counters(LogQueue *self)
{
  stats_lock();
  {
    if (self->metrics.owned.events_sc_key)
      {
        stats_unregister_counter(self->metrics.owned.events_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.owned.queued_messages);

        stats_cluster_key_free(self->metrics.owned.events_sc_key);
      }

    if (self->metrics.owned.memory_usage_sc_key)
      {
        stats_unregister_counter(self->metrics.owned.memory_usage_sc_key, SC_TYPE_SINGLE_VALUE,
                                 &self->metrics.owned.memory_usage);

        stats_cluster_key_free(self->metrics.owned.memory_usage_sc_key);
      }
  }
  stats_unlock();
}

static void
_unregister_counters(LogQueue *self)
{
  _unregister_shared_counters(self);
  _unregister_owned_counters(self);
}

void
log_queue_init_instance(LogQueue *self, const gchar *persist_name, gint stats_level,
                        StatsClusterKeyBuilder *driver_sck_builder, StatsClusterKeyBuilder *queue_sck_builder)
{
  g_atomic_counter_set(&self->ref_cnt, 1);
  self->free_fn = log_queue_free_method;

  self->persist_name = persist_name ? g_strdup(persist_name) : NULL;
  g_mutex_init(&self->lock);

  _register_counters(self, stats_level, driver_sck_builder, queue_sck_builder);
}

void
log_queue_free_method(LogQueue *self)
{
  _unregister_counters(self);
  g_mutex_clear(&self->lock);
  g_free(self->persist_name);
  g_free(self);
}
