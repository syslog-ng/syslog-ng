/*
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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

#include "kafka-source.h"
#include "kafka-internal.h"
#include "kafka-props.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/stats-cluster-single.h"
#include "stats/aggregator/stats-aggregator-registry.h"
#include "timeutils/misc.h"

#include <iv.h>

// TODO: Move to a common lib place
GList *
g_list_remove_duplicates(GList *list, GEqualFunc compare_func)
{
  g_assert(list && compare_func);

  for (GList *outer = list; outer != NULL; outer = outer->next)
    {
      GList *inner = g_list_last(list);

      while (inner != NULL && inner != outer)
        {
          GList *prev = inner->prev;

          if (compare_func(outer->data, inner->data))
            list = g_list_delete_link(list, inner);

          inner = prev;
        }
    }
  return list;
}

typedef gboolean (*ListItemConvertFunc)(const gchar *token, gpointer *item);
typedef gboolean (*ListItemValidateFunc)(gpointer token, GError **error);

// TODO: Move to a common lib place
static GList *
split_and_convert_to_list(const gchar *name, const gchar *input, const gchar *split_str,
                          gboolean break_on_validate_err,
                          GDestroyNotify free_func, ListItemConvertFunc convert_func, ListItemValidateFunc validate_func,
                          GError **error)
{
  g_assert(input && *input);

  GList *list = NULL;
  gchar *trimmed = NULL;
  int item_ndx = 0;
  gchar **tokens = g_strsplit(input, split_str, -1);

  for (; tokens[item_ndx] != NULL; item_ndx++)
    {
      trimmed = g_strstrip(tokens[item_ndx]);
      if (*trimmed == 0)
        goto err_exit;
      else
        {
          gpointer item = NULL;
          if (FALSE == convert_func(trimmed, &item))
            goto maybe_exit;
          else
            {
              if (validate_func)
                {
                  if (validate_func(item, error))
                    list = g_list_prepend(list, item);
                  else
                    {
                      if (free_func)
                        free_func(item);
                      goto maybe_exit;
                    }
                }
              else
                list = g_list_prepend(list, item);
            }
        }
      continue;
maybe_exit:
      if (break_on_validate_err)
        goto err_exit;
      else
        msg_warning("kafka: skipping invalid list item value", evt_tag_str("name", name), evt_tag_int("item_ndx", item_ndx),
                    evt_tag_str("value", trimmed));
    }
  g_strfreev(tokens);

  return list;

err_exit:
  msg_error("kafka: invalid list item value", evt_tag_str("name", name), evt_tag_int("item_ndx", item_ndx),
            evt_tag_str("value", trimmed));
  g_strfreev(tokens);
  if (list)
    g_list_free_full(list, free_func);
  return NULL;
}

static gboolean
_convert_to_int(const gchar *token, gpointer *item)
{
  char *endptr;
  long val = strtol(token, &endptr, 10);

  if (*endptr != '\0')
    {
      *item = NULL;
      return FALSE;
    }
  *item = GINT_TO_POINTER((gint)(int32_t) val);
  return TRUE;
}

static gboolean
_validate_part_num(gpointer item, GError **error)
{
  int32_t part_num = (int32_t)GPOINTER_TO_INT(item);
  return part_num >= -1;
}

static gboolean
_convert_to_string(const gchar *token, gpointer *item)
{
  *item = g_strdup(token);
  return TRUE;
}

static gboolean
_is_topic_pattern(const char *topic)
{
  if (topic == NULL || *topic == 0)
    return FALSE;
  // FIXME: ass proper pattern validation
  return kafka_is_valid_topic_pattern(topic) == FALSE;
}

static gboolean
_validate_topic_name(gpointer item, GError **error)
{
  const gchar *topic = (const gchar *)item;

  *error = NULL;
  if (_is_topic_pattern(topic))
    {
      // TODO: Add pattern validation
    }
  else
    {
      if (FALSE == kafka_validate_topic_name(topic, error))
        return FALSE;
    }
  return TRUE;
}

static void
_format_stats_key(LogThreadedSourceDriver *d, StatsClusterKeyBuilder *kb)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "kafka"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("topic",
                                             self->options.requested_topics));
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  const KafkaSourceDriver *self = (const KafkaSourceDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka-src.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka-src(%s)", self->options.requested_topics);
  return persist_name;
}

static void
_wait_for_queue(KafkaSourceDriver *self)
{
  g_mutex_lock(&self->queue_cond_mutex);
  g_atomic_counter_inc(&self->sleeping_thread_num);
  g_cond_wait(&self->queue_cond, &self->queue_cond_mutex);
  g_atomic_counter_dec_and_test(&self->sleeping_thread_num);
  g_mutex_unlock(&self->queue_cond_mutex);
}

static void
_signal_queue(KafkaSourceDriver *self)
{
  g_mutex_lock(&self->queue_cond_mutex);
  g_cond_broadcast(&self->queue_cond);
  g_mutex_unlock(&self->queue_cond_mutex);
}

static void
_log_reader_insert_msg_length_stats(KafkaSourceDriver *self, gsize len)
{
  stats_aggregator_add_data_point(self->max_message_size, len);
  stats_aggregator_add_data_point(self->average_messages_size, len);
}

static void
_register_aggregated_stats(KafkaSourceDriver *self)
{
  LogThreadedSourceWorker *worker = self->super.workers[0];
  StatsClusterKeyBuilder *kb = worker->super.metrics.stats_kb;
  stats_cluster_key_builder_push(kb);
  {
    LogSourceOptions *super_options = &self->options.super_source_options->super;
    gchar *stats_id = worker->super.stats_id;
    gchar stats_instance[1024];
    const gchar *instance_name = stats_cluster_key_builder_format_legacy_stats_instance(kb, stats_instance,
                                 sizeof(stats_instance));
    stats_aggregator_lock();
    StatsClusterKey sc_key;

    // msg_size_max
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "msg_size_max");
    stats_register_aggregator_maximum(super_options->stats_level, &sc_key,
                                      &self->max_message_size);

    // msg_size_avg
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "msg_size_avg");
    stats_register_aggregator_average(super_options->stats_level, &sc_key,
                                      &self->average_messages_size);

    // eps
    stats_cluster_single_key_legacy_set_with_name(&sc_key,
                                                  super_options->stats_source | SCS_SOURCE,
                                                  stats_id,
                                                  instance_name, "eps");
    stats_register_aggregator_cps(super_options->stats_level, &sc_key,
                                  worker->super.metrics.recvd_messages_key,
                                  SC_TYPE_SINGLE_VALUE,
                                  &self->CPS);
    stats_aggregator_unlock();
  }
  stats_cluster_key_builder_pop(kb);
}

static void
_unregister_aggregated_stats(KafkaSourceDriver *self)
{
  stats_aggregator_lock();

  stats_unregister_aggregator(&self->max_message_size);
  stats_unregister_aggregator(&self->average_messages_size);
  stats_unregister_aggregator(&self->CPS);

  stats_aggregator_unlock();
}

static gsize
_log_message_from_string(const char *msg_cstring, MsgFormatOptions *format_options, LogMessage **out_msg)
{
  gsize msg_len = strlen(msg_cstring);
  *out_msg = msg_format_construct_message(format_options, (const guchar *) msg_cstring, msg_len);
  msg_format_parse_into(format_options, *out_msg, (const guchar *) msg_cstring, msg_len);
  return msg_len;
}

static void
_decide_strategy(KafkaSourceDriver *self)
{
  gboolean single_topic = g_list_length(self->requested_topics) == 1 &&
                          FALSE == _is_topic_pattern(g_list_first(self->requested_topics)->data);
  gboolean single_partition = g_list_length(self->requested_partitions) == 1 &&
                              (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data) != -1;
  gboolean wildcard_partition = g_list_length(self->requested_partitions) == 1 &&
                                (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data) == -1;

  if (single_topic && single_partition)
    self->startegy = KSCS_BATCH_POLL;
  else if (wildcard_partition)
    self->startegy = KSCS_SUBSCRIBE_POLL;
  else
    self->startegy = KSCS_ASSIGN_POLL;

  msg_verbose("kafka: selected consumer startegy",
              evt_tag_str("strategy", self->startegy == KSCS_BATCH_POLL ? "batch_poll" :
                          self->startegy == KSCS_SUBSCRIBE_POLL ? "subscribe_poll" : "assign_poll"),
              evt_tag_str("group_id", self->group_id),
              evt_tag_str("driver", self->super.super.super.id));
}

static inline void
_send(LogThreadedSourceWorker *worker, LogMessage *msg)
{
  log_threaded_source_worker_blocking_post(worker, msg);
}

static inline gdouble
_iteration_sleep_time(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  return self->options.fetch_delay ? 1.0 / self->options.fetch_delay : 0;
}

static void _kafka_throttle_cb(rd_kafka_t *rk, const char *broker_name,
                               int32_t broker_id, int throttle_time_ms, void *opaque)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) ((KafkaOpaque *)opaque)->driver;

  msg_info("kafka: Broker throttled request",
           evt_tag_str("group_id", self->group_id),
           evt_tag_str("broker_name", broker_name),
           evt_tag_int("throttle_time_ms", throttle_time_ms),
           evt_tag_str("driver", self->super.super.super.id));
}

static gboolean
_process_message(LogThreadedSourceWorker *worker, rd_kafka_message_t *msg)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  msg_trace("kafka: processing message",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
            evt_tag_int("partition", msg->partition),
            evt_tag_str("message", (gchar *)msg->payload),
            evt_tag_str("driver", self->super.super.super.id));

  LogMessage *log_msg;
  gsize msg_len = _log_message_from_string((gchar *)msg->payload, self->options.format_options, &log_msg);
  log_msg_set_value_to_string(log_msg, LM_V_TRANSPORT, "local+kafka");
  _log_reader_insert_msg_length_stats(self, msg_len);

  _send(worker, log_msg);

  if (self->startegy == KSCS_SUBSCRIBE_POLL || self->startegy == KSCS_ASSIGN_POLL)
    {
      rd_kafka_error_t *err = rd_kafka_offset_store_message(msg);
      if (err)
        msg_warning("kafka: error storing message offset",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                    evt_tag_int("partition", msg->partition),
                    evt_tag_str("error", rd_kafka_error_string(err)),
                    evt_tag_str("driver", self->super.super.super.id));
      if (rd_kafka_commit_message(self->kafka, msg, FALSE) != RD_KAFKA_RESP_ERR_NO_ERROR)
        msg_warning("kafka: error commiting message offset",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                    evt_tag_int("partition", msg->partition),
                    evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())),
                    evt_tag_str("driver", self->super.super.super.id));
    }
  rd_kafka_message_destroy(msg);

  return TRUE;
}

static void
_drop_queued_messages(KafkaSourceDriver *self)
{
  rd_kafka_message_t *msg;
  while ((msg = g_async_queue_try_pop(self->msg_queue)) != NULL)
    rd_kafka_message_destroy(msg);
}

static ThreadedFetchResult
_fetch(LogThreadedSourceWorker *worker, rd_kafka_message_t **msg)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  if ((*msg = g_async_queue_try_pop(self->msg_queue)) == NULL)
    return THREADED_FETCH_NO_DATA;

  return THREADED_FETCH_SUCCESS;
}

/* runs in a dedicated thread */
static void
_procesor_run(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  /* KSCS_BATCH_POLL can run on a single thread only to guarantee strict ordering; the thread count must have already been set to 1. */
  g_assert(self->startegy != KSCS_BATCH_POLL);

  g_atomic_counter_inc(&self->running_thread_num);

  msg_debug("kafka: started queue processor",
            evt_tag_int("index", worker->worker_index),
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));

  const gdouble iteration_sleep_time = _iteration_sleep_time(worker);
  rd_kafka_message_t *msg = NULL;

  while (FALSE == main_loop_worker_job_quit())
    {
      if (g_async_queue_length(self->msg_queue) == 0)
        _wait_for_queue(self);

      while (FALSE == main_loop_worker_job_quit())
        {
          ThreadedFetchResult fetch_result = _fetch(worker, &msg);
          if (fetch_result == THREADED_FETCH_SUCCESS)
            {
              _process_message(worker, msg);
              if (iteration_sleep_time > 0.0)
                main_loop_worker_wait_for_exit_until(iteration_sleep_time);
            }
          else if (fetch_result == THREADED_FETCH_NO_DATA)
            {
              // TODO:
              break;
            }
          else
            // TODO:
            ;
        }

      if (FALSE == main_loop_worker_job_quit() && self->options.fetch_retry_delay)
        main_loop_worker_wait_for_exit_until(self->options.fetch_retry_delay);
    }
  msg_debug("kafka: stopped queue processor",
            evt_tag_int("index", worker->worker_index),
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));

  g_atomic_counter_dec_and_test(&self->running_thread_num);
}

/* **************************
 * Strategy - assign poll
 * **************************
 */
static gboolean
_setup_method_assign_consumer_poll(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  const guint parts_num = g_list_length(self->requested_partitions);
  int32_t first_requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data);
  g_assert(parts_num >= 1 && first_requested_partition != -1 && topics_num > 0);

  /* Forward main event queue to consumer queue so we can serve both queues with a single consumer_poll() call */
  rd_kafka_poll_set_consumer(self->kafka);

  rd_kafka_topic_partition_list_t *parts = rd_kafka_topic_partition_list_new(g_list_length(self->requested_topics));
  for (GList *t = self->requested_topics; t; t = t->next)
    {
      const gchar *requested_topic = (const gchar *)(t->data);
      for (GList *p = self->requested_partitions; p; p = p->next)
        {
          int32_t requested_partition = (int32_t)GPOINTER_TO_INT(p->data);
          rd_kafka_topic_partition_list_add(parts, requested_topic, requested_partition);
        }
    }
  rd_kafka_resp_err_t err;
  if ((err = rd_kafka_assign(self->kafka, parts)) == RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_debug("kafka: partitions assigned",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("driver", self->super.super.super.id));
      self->assigned_partitions = parts;
    }
  else
    {
      msg_error("kafka: rd_kafka_assign() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id));
      rd_kafka_topic_partition_list_destroy(parts);
      result = FALSE;
    }

  return result;
}

/* **************************
 * Strategy - subscribe poll
 * **************************
 */
static gboolean
_setup_method_subscribe_consumer_poll(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  const guint parts_num = g_list_length(self->requested_partitions);
  int32_t requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data);
  g_assert(parts_num == 1 && requested_partition == -1 && topics_num > 0);

  /* Forward main event queue to consumer queue so we can serve both queues with a single consumer_poll() call */
  rd_kafka_poll_set_consumer(self->kafka);

  rd_kafka_topic_partition_list_t *parts = rd_kafka_topic_partition_list_new(g_list_length(self->requested_topics));
  for (GList *t = self->requested_topics; t; t = t->next)
    {
      const gchar *requested_topic = (const gchar *)(t->data);
      rd_kafka_topic_partition_list_add(parts, requested_topic, RD_KAFKA_PARTITION_UA); // partition is ignored in subscribe
    }
  /* Subscribe to topic set using balanced consumer groups.
   * Wildcard (regex) topics are supported: any topic name in the topics list that is prefixed with "^" will be regex-matched to the full list of topics in the cluster and matching topics will be added to the subscription list.
   * The full topic list is retrieved every topic.metadata.refresh.interval.ms to pick up new or delete topics that match the subscription. If there is any change to the matched topics the consumer will immediately rejoin the group with the updated set of subscribed topics.
   * Regex and full topic names can be mixed in topics.
   * Remarks
   * Only the .topic field is used in the supplied topics list, all other fields are ignored.
   * subscribe() is an asynchronous method which returns immediately: background threads will (re)join the group, wait for group rebalance, issue any registered rebalance_cb, assign() the assigned partitions, and then start fetching messages. This cycle may take up to session.timeout.ms * 2 or more to complete.
   */
  rd_kafka_resp_err_t err;
  if ((err = rd_kafka_subscribe(self->kafka, parts)) == RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_debug("kafka: waiting for group rebalancer",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("driver", self->super.super.super.id));
      self->assigned_partitions = parts;
    }
  else
    {
      msg_error("kafka: rd_kafka_subscribe() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id));
      rd_kafka_topic_partition_list_destroy(parts);
      result = FALSE;
    }

  return result;
}

/* runs in a dedicated thread */
static void
_consumer_run_consumer_poll(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  rd_kafka_message_t *msg;

  msg_debug("kafka: started consumer poll run - queued",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));

  while (FALSE == main_loop_worker_job_quit())
    {
      int qlen = (int)rd_kafka_outq_len(self->kafka);

      msg = rd_kafka_consumer_poll(self->kafka, self->options.super.poll_timeout);
      if (msg == NULL || msg->err)
        {
          if (msg == NULL || msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
            {
              // msg_trace("kafka: consumer_poll timeout",
              //           evt_tag_int("kafka_outq_len", qlen),
              //           evt_tag_int("msg_queue_len", g_async_queue_length(self->msg_queue)));
              if (msg)
                rd_kafka_message_destroy(msg);
              if (self->options.fetch_retry_delay)
                main_loop_worker_wait_for_exit_until(self->options.fetch_retry_delay);
            }
          else
            {
              msg_error("kafka: consumer poll message error",
                        evt_tag_str("group_id", self->group_id),
                        evt_tag_str("topic", self->options.requested_topics),
                        evt_tag_str("partition", self->options.requested_partitions),
                        evt_tag_str("error", rd_kafka_err2str(msg->err)),
                        evt_tag_str("driver", self->super.super.super.id));
              rd_kafka_message_destroy(msg);
              break;
            }
        }
      else
        {
          g_async_queue_push(self->msg_queue, msg);
          msg_trace("kafka: got message",
                    evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                    evt_tag_int("partition", msg->partition),
                    evt_tag_int("offset", msg->offset),
                    evt_tag_int("kafka_outq_len", qlen),
                    evt_tag_int("msg_queue_len", g_async_queue_length(self->msg_queue)));
          _signal_queue(self);

          // Let this take effect in the processor?!?
          // if (iteration_sleep_time > 0.0)
          //   main_loop_worker_wait_for_exit_until(iteration_sleep_time);
        }
    }
  /* Wait for outstanding requests to finish. */
  // FIXME: clarify if we need rd_kafka_flush(). and polling rd_kafka_outq_len() till its empty, if we only have offset commits
}

/* ***********************
 * Strategy - batch poll
 * ***********************
 */
static gboolean
_setup_method_batch_consumer_poll(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  const guint parts_num = g_list_length(self->requested_partitions);
  g_assert(topics_num == 1 && parts_num == 1);

  int32_t requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data);
  const gchar *requested_topic = (const gchar *)(g_list_first(self->requested_topics)->data);

  rd_kafka_topic_t *new_topic = rd_kafka_topic_new(self->kafka,
                                                   requested_topic,
                                                   NULL); // TODO: add support of per-topic conf
  if (rd_kafka_consume_start(new_topic,
                             requested_partition,
                             RD_KAFKA_OFFSET_END) != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_error("kafka: rd_kafka_consume_start() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())),
                evt_tag_str("driver", self->super.super.super.id));
      rd_kafka_topic_destroy(new_topic);
      result = FALSE;
    }
  else
    self->topic_handle_list = g_list_append(self->topic_handle_list, new_topic);

  return result;
}

/* runs in a dedicated thread */
static void
_consumer_run_batch_poll(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  g_assert(g_list_length(self->requested_partitions) == 1 && g_list_length(self->topic_handle_list) == 1);
  // TODO: make this dynamic
#define MAX_BATCH_SIZE 100
  rd_kafka_message_t *msgs[MAX_BATCH_SIZE];
  int32_t requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(self->requested_partitions)->data);
  rd_kafka_topic_t *single_topic = (rd_kafka_topic_t *)g_list_first(self->topic_handle_list)->data;

  msg_debug("kafka: started consumer poll run - NONE queued",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));

  while (FALSE == main_loop_worker_job_quit())
    {
      /* Polls the provided kafka handle for events. Events will cause application-provided callbacks to be called.
       * For non-blocking calls, provide 0 as timeout_ms. To wait indefinitely for an event, provide -1. */
      rd_kafka_poll(self->kafka, 0);

      ssize_t cnt = rd_kafka_consume_batch(single_topic, requested_partition, self->options.super.poll_timeout,
                                           msgs,
                                           MAX_BATCH_SIZE);
      if (cnt < 0 || (cnt == 1 && msgs[0]->err))
        {
          msg_error("kafka: consume batch error",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("topic", self->options.requested_topics),
                    evt_tag_str("partition", self->options.requested_partitions),
                    evt_tag_str("error", cnt < 0 ? rd_kafka_err2str(rd_kafka_last_error()) : rd_kafka_message_errstr(msgs[0])),
                    evt_tag_str("driver", self->super.super.super.id));
          break;
        }

      for (ssize_t i = 0; i < cnt; i++)
        {
          rd_kafka_message_t *msg = msgs[i];
          if (FALSE == main_loop_worker_job_quit())
            {
              _process_message(worker, msg);
              if (iteration_sleep_time > 0.0)
                main_loop_worker_wait_for_exit_until(iteration_sleep_time);
            }
          else
            rd_kafka_message_destroy(msg);
        }

      if (cnt == 0 && self->options.fetch_retry_delay && FALSE == main_loop_worker_job_quit())
        main_loop_worker_wait_for_exit_until(self->options.fetch_retry_delay);
    }
  rd_kafka_consume_stop(single_topic, requested_partition);

  /* Wait for outstanding requests to finish. */
  // FIXME: clarify if we need rd_kafka_flush(). and polling rd_kafka_outq_len() till its empty, if we only have offset commits
}

static gboolean
_restart_consumer(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  main_loop_worker_wait_for_exit_until(self->options.time_reopen);

  while (FALSE == main_loop_worker_job_quit())
    {
      if (g_atomic_counter_get(&self->sleeping_thread_num) == self->super.num_workers - 1)
        {
          kafka_sd_reopen(&self->super.super.super);
          break;
        }
      msg_trace("kafka: waiting for queue processors to sleep",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("driver", self->super.super.super.id));
      if (iteration_sleep_time > 0.0)
        main_loop_worker_wait_for_exit_until(iteration_sleep_time);
    }

  return FALSE == main_loop_worker_job_quit();
}

/* runs in a dedicated thread */
static void
_consumer_run(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  const gdouble iteration_sleep_time = _iteration_sleep_time(worker);
  gboolean exit_requested = FALSE;

  g_atomic_counter_inc(&self->running_thread_num);
  do
    {
      switch(self->startegy)
        {
        case KSCS_ASSIGN_POLL:
        case KSCS_SUBSCRIBE_POLL:
          _consumer_run_consumer_poll(worker, iteration_sleep_time);
          break;

        case KSCS_BATCH_POLL:
          _consumer_run_batch_poll(worker, iteration_sleep_time);
          break;
        default:
          g_assert_not_reached();
        }
      msg_debug("kafka: stopped consumer poll run",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("driver", self->super.super.super.id));

      exit_requested = main_loop_worker_job_quit();

      /* this means an error happened, let's retry from ground */
      if (FALSE == exit_requested)
        exit_requested = (FALSE == _restart_consumer(worker, iteration_sleep_time));
    }
  while (FALSE == exit_requested);

  /* Wake-up sleeping/waiting (queue) processor workers */
  _signal_queue(self);
  /* Wait for all the (queue) processor workers to exit, see _procesor_run why not just for the _consumer_run_consumer_poll case */
  while (g_atomic_counter_get(&self->running_thread_num) > 1)
    {
      main_loop_worker_wait_for_exit_until(iteration_sleep_time);
      _signal_queue(self);
    }

  if (self->startegy != KSCS_BATCH_POLL)
    _drop_queued_messages(self);

  g_atomic_counter_dec_and_test(&self->running_thread_num);
}

static gboolean
_adjust_num_workers(KafkaSourceDriver *self)
{
  gboolean result = TRUE;

  if (self->startegy == KSCS_BATCH_POLL)
    {
      msg_warning("kafka: setting worker thread count to 1 for the selected processing strategy",
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("topic", self->options.requested_topics),
                  evt_tag_str("partition", self->options.requested_partitions),
                  evt_tag_str("driver", self->super.super.super.id));
      log_threaded_source_driver_set_num_workers(&self->super.super.super, 1);
    }
  else if (self->super.num_workers < 2)
    {
      msg_error("kafka: the selected processing strategy requires at least 2 worker threads",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("driver", self->super.super.super.id));
      result = FALSE;
    }
  return result;
}

static gboolean
_setup_kafka_client(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;

  _decide_strategy(self);

  if (FALSE == _adjust_num_workers(self))
    return FALSE;

  switch(self->startegy)
    {
    case KSCS_ASSIGN_POLL:
      result = _setup_method_assign_consumer_poll(self);
      break;
    case KSCS_SUBSCRIBE_POLL:
      result = _setup_method_subscribe_consumer_poll(self);
      break;
    case KSCS_BATCH_POLL:
      result = _setup_method_batch_consumer_poll(self);
      break;
    default:
      g_assert_not_reached();
    }
  return result;
}

static void
_kafka_rebalance_cb(rd_kafka_t *rk,
                    rd_kafka_resp_err_t err,
                    rd_kafka_topic_partition_list_t *partitions,
                    void *opaque)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) ((KafkaOpaque *)opaque)->driver;

  g_assert(self->kafka == rk);
  switch (err)
    {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
      msg_verbose("kafka: group rebalanced - assigned",
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("member_id", rd_kafka_memberid(rk)));
      if (self->assigned_partitions)
        rd_kafka_topic_partition_list_destroy(self->assigned_partitions);
      self->assigned_partitions = rd_kafka_topic_partition_list_copy(partitions);
      kafka_log_partition_list(self->assigned_partitions);
      /* Broker assigned the partitions → assign to the consumers too */
      rd_kafka_assign(self->kafka, self->assigned_partitions);
      break;

    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
      msg_verbose("kafka: group rebalanced - revoked",
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("member_id", rd_kafka_memberid(rk)));
      kafka_log_partition_list(partitions);
      if (self->assigned_partitions)
        {
          rd_kafka_topic_partition_list_destroy(self->assigned_partitions);
          self->assigned_partitions = NULL;
        }
      /* Revoke partitions from the consumers */
      rd_kafka_assign(self->kafka, NULL);
      break;

    default:
      msg_error("kafka: rebalance error",
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("group_id", self->group_id));
      break;
    }
}

static gboolean
_check_and_apply_topics(KafkaSourceDriver *self, const gchar *topics, gboolean apply)
{
  GError *error = NULL;
  GList *list = split_and_convert_to_list("topic", topics, ",", TRUE, g_free, _convert_to_string, _validate_topic_name,
                                          &error);
  if (error)
    {
      msg_error("kafka: invalid topic name in the requested topics list",
                evt_tag_str("topics", topics),
                evt_tag_str("error", error->message),
                evt_tag_str("driver", self->super.super.super.id));
      g_error_free(error);
      return FALSE;
    }
  const gint original_length = g_list_length(list);
  if (list)
    list = g_list_remove_duplicates(list, g_str_equal);
  if (list == NULL || g_list_length(list) == 0)
    {
      if (list)
        g_list_free_full(list, g_free);
      list = NULL;
    }
  if (list == NULL)
    return FALSE;

  if (apply)
    {
      if (original_length != g_list_length(list))
        msg_warning("kafka: dropped duplicated entries of topics",
                    evt_tag_str("topics", topics),
                    evt_tag_str("driver", self->super.super.super.id));
      if (self->requested_topics)
        g_list_free_full(self->requested_topics, g_free);
      self->requested_topics = list;
    }
  return TRUE;
}

static gboolean
_g_int32_equal(gconstpointer  v1, gconstpointer  v2)
{
  return (int32_t)GPOINTER_TO_INT(v1) == (int32_t)GPOINTER_TO_INT(v2);
}

static gboolean
_check_and_apply_partitions(KafkaSourceDriver *self, const gchar *partitions, gboolean apply)
{
  GList *list = split_and_convert_to_list("partition", partitions, ",", TRUE, NULL, _convert_to_int,
                                          _validate_part_num, NULL);
  const gint original_length = g_list_length(list);
  if (list)
    list = g_list_remove_duplicates(list, _g_int32_equal);
  if (list == NULL || g_list_length(list) == 0)
    {
      if (list)
        g_list_free(list);
      list = NULL;
    }
  if (list == NULL)
    return FALSE;

  const gpointer all_parts = GINT_TO_POINTER((gint)(int32_t)-1);
  if (g_list_find(list, all_parts))
    {
      if (g_list_length(list) > 1)
        {
          msg_error("kafka: error, 'all partitions' (-1) specified along with other partition numbers",
                    evt_tag_str("partitions", partitions),
                    evt_tag_str("driver", self->super.super.super.id));
          return FALSE;
        }
    }

  if (apply)
    {
      if (original_length != g_list_length(list))
        msg_warning("kafka: dropped duplicated entries of partition numbers",
                    evt_tag_str("partitions", partitions),
                    evt_tag_str("driver", self->super.super.super.id));
      if (self->requested_partitions)
        g_list_free(self->requested_partitions);
      self->requested_partitions = list;
    }
  return TRUE;
}

static gint
_property_name_compare(const KafkaProperty *kp1, const KafkaProperty *kp2)
{
  return g_strcmp0(kp1->name, kp2->name);
}

static void
_apply_group_id(KafkaSourceDriver *self)
{
  gchar *conf_group_id = NULL;
  const KafkaProperty key = { "group.id", NULL };
  GList *li = g_list_find_custom(self->options.super.config, &key, (GCompareFunc) _property_name_compare);
  if (li == NULL)
    {
      conf_group_id = g_strdup(self->super.super.super.id);
      KafkaProperty *kp_groupid = g_new0(KafkaProperty, 1);
      kp_groupid->name = g_strdup("group.id");
      kp_groupid->value = g_strdup(conf_group_id);
      self->options.super.config = g_list_prepend(self->options.super.config, kp_groupid);
      msg_trace("kafka: config group.id not found, using self-generated group id",
                evt_tag_str("group_id", conf_group_id),
                evt_tag_str("driver", self->super.super.super.id));
    }
  else
    {
      KafkaProperty *conf_data = ((KafkaProperty *)li->data);
      if (conf_data->value == NULL || conf_data->value[0] == '\0')
        {
          self->options.super.config = g_list_remove(self->options.super.config, li->data);
          msg_warning("kafka: config group.id is empty, forcibly NOT using group id, removed from config",
                      evt_tag_str("driver", self->super.super.super.id));
        }
      else
        {
          conf_group_id = g_strdup(conf_data->value);
          msg_trace("kafka: using config group id",
                    evt_tag_str("group_id", conf_group_id),
                    evt_tag_str("driver", self->super.super.super.id));
        }
    }
  g_free(self->group_id);
  self->group_id = conf_group_id;
}

static void
_apply_options(KafkaSourceDriver *self)
{
  _apply_group_id(self);
  _check_and_apply_topics(self, self->options.requested_topics, TRUE);
  _check_and_apply_partitions(self, self->options.requested_partitions, TRUE);
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  LogThreadedSourceWorker *worker = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(worker, s, worker_index);
  if (worker_index == 0)
    worker->run = _consumer_run;
  else
    worker->run = _procesor_run;
  return worker;
}

static rd_kafka_t *
_construct_kafka_client(KafkaSourceDriver *self)
{
  _apply_options(self);

  msg_debug("kafka: constructing client",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));

  rd_kafka_conf_t *conf = rd_kafka_conf_new();

  if (!kafka_conf_set_prop(conf, "metadata.broker.list", self->options.super.bootstrap_servers))
    goto err_exit;
  /* NOTE: The callback-based consumer API's offset store granularity is
   * said to be not good enough, also, we want to control the offset store
   * process, and treat the message process if we were able to fuly deliver it.
   * Disable automatic offset store and commiting, do it explicitly per-message in
   * the message processor of the consume callback/batch_loop instead.
   * Also, this controls only the high-level consumer API scenario, for the low-lewel
   * version we should use our persist state backup/restore and RD_KAFKA_OFFSET_STORED
   * in rd_kafka_consume_start */
  if (!kafka_conf_set_prop(conf, "enable.auto.offset.store", "false"))
    goto err_exit;
  if (!kafka_conf_set_prop(conf, "enable.auto.commit", "false"))
    goto err_exit;
  /* Like RD_KAFKA_OFFSET_END of rd_kafka_consume_start for the high-level consumer API
   * this is just a fallback if the offset cannot be restored */
  if (!kafka_conf_set_prop(conf, "auto.offset.reset", "latest"))
    goto err_exit;

  static gchar *protected_properties[] =
  {
    "bootstrap.servers",
    "metadata.broker.list",
    "enable.auto.offset.store",
    "enable.auto.commit",
    "auto.commit.enable",
  };
  if (FALSE == kafka_apply_config_props(conf, self->options.super.config, protected_properties,
                                        G_N_ELEMENTS(protected_properties)))
    goto err_exit;

  rd_kafka_conf_set_log_cb(conf, kafka_log_callback);
  rd_kafka_conf_set_opaque(conf, &self->opaque);
  rd_kafka_conf_set_throttle_cb(conf, _kafka_throttle_cb);
  rd_kafka_conf_set_rebalance_cb(conf, _kafka_rebalance_cb);

  gchar errbuf[1024] = {0};
  rd_kafka_t *client = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errbuf, sizeof(errbuf));
  if (NULL == client)
    {
      msg_error("kafka: error constructing the kafka connection object",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("error", errbuf),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
  return client;

err_exit:
  rd_kafka_conf_destroy(conf);
  return NULL;
}

static void
_kafka_topic_free_func(gpointer data)
{
  rd_kafka_topic_t *topic_handle = (rd_kafka_topic_t *)data;
  rd_kafka_topic_destroy(topic_handle);
}

static void
_destroy_kafka_client(LogDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  msg_debug("kafka: destroying client",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", self->options.requested_topics),
            evt_tag_str("partition", self->options.requested_partitions),
            evt_tag_str("driver", self->super.super.super.id));
  if  (self->topic_handle_list)
    {
      g_list_free_full(self->topic_handle_list, _kafka_topic_free_func);
      self->topic_handle_list = NULL;
    }

  if (self->assigned_partitions)
    {
      rd_kafka_topic_partition_list_destroy(self->assigned_partitions);
      self->assigned_partitions = NULL;
    }

  if (self->kafka_queue)
    {
      rd_kafka_queue_destroy(self->kafka_queue);
      self->kafka_queue = NULL;
    }

  if (self->kafka)
    {
      if (self->startegy == KSCS_SUBSCRIBE_POLL)
        {
          rd_kafka_unsubscribe(self->kafka);
          rd_kafka_consumer_close(self->kafka);
        }
      else if (self->startegy != KSCS_BATCH_POLL)
        {
          rd_kafka_assign(self->kafka, NULL);
        }

      rd_kafka_destroy(self->kafka);
      self->kafka = NULL;
    }

  if (self->options.super.config)
    kafka_property_list_free(self->options.super.config);
  self->options.super.config = NULL;

  g_free(self->group_id);
  self->group_id = NULL;

  if (self->requested_partitions)
    g_list_free(self->requested_partitions);
  self->requested_partitions = NULL;

  if (self->requested_topics)
    g_list_free_full(self->requested_topics, g_free);
  self->requested_topics = NULL;
}

gboolean
kafka_sd_reopen(LogDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  if (self->kafka)
    _destroy_kafka_client(s);

  self->kafka = _construct_kafka_client(self);
  if (self->kafka == NULL)
    {
      msg_error("kafka: error constructing kafka connection object",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", self->options.requested_topics),
                evt_tag_str("partition", self->options.requested_partitions),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  if (FALSE == _setup_kafka_client(self))
    {
      _destroy_kafka_client(s);
      return FALSE;
    }
  return TRUE;
}

static gboolean
kafka_sd_init(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->options.requested_topics == NULL)
    {
      msg_error("kafka: the topic() argument is required for kafka source",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  if (self->options.super.bootstrap_servers == NULL)
    {
      msg_error("kafka: the bootstrap-servers() option is required for kafka source",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  if (FALSE == kafka_sd_reopen(&self->super.super.super))
    return FALSE;

  if (FALSE == log_threaded_source_driver_init_method(s))
    return FALSE;

  _register_aggregated_stats(self);

  self->msg_queue = g_async_queue_new();
  g_cond_init(&self->queue_cond);
  g_mutex_init(&self->queue_cond_mutex);

  log_template_options_init(&self->options.template_options, cfg);
  msg_verbose("kafka: Kafka source initialized",
              evt_tag_str("group_id", self->group_id),
              evt_tag_str("topic", self->options.requested_topics),
              evt_tag_str("partition", self->options.requested_partitions),
              evt_tag_str("driver", self->super.super.super.id),
              log_pipe_location_tag(&self->super.super.super.super));
  return TRUE;
}

static gboolean
kafka_sd_deinit(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  g_assert(g_async_queue_length(self->msg_queue) == 0);
  g_async_queue_unref(self->msg_queue);
  g_mutex_clear(&self->queue_cond_mutex);
  g_cond_clear(&self->queue_cond);

  _destroy_kafka_client(&self->super.super.super);

  _unregister_aggregated_stats(self);

  return log_threaded_source_driver_deinit_method(s);
}

static void
kafka_sd_free(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  kafka_sd_options_destroy(&self->options);

  log_threaded_source_driver_free_method(s);
}

LogDriver *
kafka_sd_new(GlobalConfig *cfg)
{
  KafkaSourceDriver *self = g_new0(KafkaSourceDriver, 1);
  log_threaded_source_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = kafka_sd_init;
  self->super.super.super.super.deinit = kafka_sd_deinit;
  self->super.super.super.super.free_fn = kafka_sd_free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  // FIXME: consecutive_ack_tracker_factory_new(); crashing on reload
  //            return consecutive_ack_record_container_static_new(log_source_get_init_window_size(source));
  //                ring_buffer_alloc(&self->ack_records, sizeof(ConsecutiveAckRecord), size);
  //                  g_assert(capacity > 0);
  self->super.worker_options.ack_tracker_factory =
    instant_ack_tracker_bookmarkless_factory_new();// consecutive_ack_tracker_factory_new();
  self->super.worker_construct = _construct_worker;

  self->super.format_stats_key = _format_stats_key;

  self->opaque.driver = &self->super.super.super;
  self->opaque.options = &self->options.super;

  kafka_sd_options_defaults(&self->options, &self->super.worker_options);

  return &self->super.super.super;
}

/* Options */

void
kafka_sd_merge_config(LogDriver *d, GList *props)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_merge_config(&self->options.super, props);
}

gboolean
kafka_sd_set_topics(LogDriver *d, const gchar *topics)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) d;

  if (FALSE == _check_and_apply_topics(self, topics, FALSE))
    return FALSE;

  g_free(self->options.requested_topics);
  self->options.requested_topics = g_strdup(topics);

  return TRUE;
}

gboolean
kafka_sd_set_partitions(LogDriver *d, const gchar *partitions)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) d;

  if (FALSE == _check_and_apply_partitions(self, partitions, FALSE))
    return FALSE;

  g_free(self->options.requested_partitions);
  self->options.requested_partitions = g_strdup(partitions);

  return TRUE;
}

void
kafka_sd_set_bootstrap_servers(LogDriver *d, const gchar *bootstrap_servers)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_set_bootstrap_servers(&self->options.super, bootstrap_servers);
}

void
kafka_sd_set_poll_timeout(LogDriver *d, gint poll_timeout)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_set_poll_timeout(&self->options.super, poll_timeout);
}

void
kafka_sd_set_time_reopen(LogDriver *d, gint time_reopen)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  self->options.time_reopen = time_reopen;
}

void
kafka_sd_set_message_ref(LogDriver *d, LogTemplate *message)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  log_template_unref(self->options.message);
  self->options.message = message;
}

LogTemplateOptions *
kafka_sd_get_template_options(LogDriver *d)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  return &self->options.template_options;
}

void
kafka_sd_set_do_not_use_bookmark(LogDriver *s, gboolean new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.do_not_use_bookmark = new_value;
}

void
kafka_sd_set_log_fetch_delay(LogDriver *s, guint new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.fetch_delay = new_value;
}

void
kafka_sd_set_log_fetch_retry_delay(LogDriver *s, guint new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.fetch_retry_delay = new_value;
}

void
kafka_sd_set_log_fetch_limit(LogDriver *s, guint new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.fetch_limit = new_value;
}

void
kafka_sd_options_defaults(KafkaSourceOptions *self,
                          LogThreadedSourceWorkerOptions *super_source_options)
{
  self->super_source_options = super_source_options;
  self->super_source_options->super.stats_level = STATS_LEVEL0;
  self->super_source_options->super.stats_source = stats_register_type("kafka_src");

  /* No additional format options now, so intentionally referencing only, and not using msg_format_options_copy */
  self->format_options = &self->super_source_options->parse_options;

  kafka_options_defaults(&self->super);
  self->time_reopen = 60;

  self->do_not_use_bookmark = FALSE;
  self->fetch_delay = 10000;
  self->fetch_limit = 0; /* 0 means no limit */
  self->fetch_retry_delay = 1; /* seconds */

  log_template_options_defaults(&self->template_options);
}

void
kafka_sd_options_destroy(KafkaSourceOptions *self)
{
  g_free(self->requested_topics);
  self->requested_topics = NULL;
  g_free(self->requested_partitions);
  self->requested_partitions = NULL;
  log_template_unref(self->message);
  self->message = NULL;

  kafka_options_destroy(&self->super);
  log_template_options_destroy(&self->template_options);

  /* Just referenced, not copied */
  self->format_options = NULL;
  self->super_source_options = NULL;
}
