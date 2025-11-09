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

#include "kafka-source-driver.h"
#include "kafka-source-worker.h"
#include "kafka-internal.h"
#include "kafka-props.h"
#include "kafka-topic-parts.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/stats-cluster-single.h"
#include "stats/aggregator/stats-aggregator-registry.h"
#include "timeutils/misc.h"

// TODO: Move to a common lib place
GList *
g_list_remove_duplicates(GList *list, GEqualFunc compare_func, GDestroyNotify free_func)
{
  g_assert(list && compare_func);

  for (GList *outer = list; outer != NULL; outer = outer->next)
    {
      GList *inner = g_list_last(list);

      while (inner != NULL && inner != outer)
        {
          GList *prev = inner->prev;

          if (compare_func(outer->data, inner->data))
            {
              if (free_func)
                free_func(inner->data);
              list = g_list_delete_link(list, inner);
            }

          inner = prev;
        }
    }
  return list;
}

typedef gboolean (*ListItemConvertFunc)(const gchar *token, gpointer *item);
typedef gboolean (*ListItemValidateFunc)(gpointer token, GError **error);

static GList *
split_and_convert_to_list(const gchar *name, const gchar *input, const gchar *split_str,
                          gboolean break_on_validate_err,
                          GDestroyNotify free_func, ListItemConvertFunc convert_func, ListItemValidateFunc validate_func)
{
  g_assert(input);
  if (*input == 0)
    {
      msg_error("kafka: list cannot be an empty string", evt_tag_str("name", name));
      return NULL;
    }

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
                  if (validate_func(item, NULL))
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
    g_list_free(list);
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
  return part_num >= RD_KAFKA_PARTITION_UA;
}

static gboolean
_is_topic_pattern(const char *topic)
{
  return FALSE == kafka_validate_topic_name(topic, NULL);
}

static gboolean
_validate_topic_name(gpointer item, GError **error)
{
  const gchar *topic = (const gchar *)item;

  if (error)
    *error = NULL;
  if (FALSE == kafka_validate_topic_name(topic, NULL))
    return kafka_validate_topic_pattern(topic, error);
  return TRUE;
}

static void
_format_stats_key(LogThreadedSourceDriver *d, StatsClusterKeyBuilder *kb)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "kafka"));
  if (self->group_id)
    stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("group_id", self->group_id));
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  const KafkaSourceDriver *self = (const KafkaSourceDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka(%s)", self->super.super.super.id);
  return persist_name;
}

void
kafka_sd_update_msg_length_stats(KafkaSourceDriver *self, gsize len)
{
  stats_aggregator_add_data_point(self->max_message_size, len);
  stats_aggregator_add_data_point(self->average_messages_size, len);
}

void
kafka_sd_inc_msg_topic_stats(KafkaSourceDriver *self, const gchar *topic)
{
  StatsCounterItem *counter = g_hash_table_lookup(self->stats_topics, topic);
  stats_counter_inc(counter);
}

void
kafka_sd_inc_msg_worker_stats(KafkaSourceDriver *self, const gchar *worker_id)
{
  StatsCounterItem *counter = g_hash_table_lookup(self->stats_workers, worker_id);
  stats_counter_inc(counter);
}

void
kafka_sd_dec_msg_worker_stats(KafkaSourceDriver *self, const gchar *worker_id)
{
  StatsCounterItem *counter = g_hash_table_lookup(self->stats_workers, worker_id);
  stats_counter_dec(counter);
}

static void
_register_worker_stats(KafkaSourceDriver *self)
{
  const gchar *counter_names[] = { "queued", NULL };

  for (int i = 1 ; i < self->used_queue_num; i++)
    {
      const gchar *worker_id_str = kafka_src_worker_get_name(self->super.workers[i]);
      if (FALSE == g_hash_table_contains(self->stats_workers, worker_id_str))
        kafka_register_counters(self, self->stats_workers, "worker", worker_id_str, counter_names, STATS_LEVEL2);
    }
}

static void
_unregister_worker_counters(gpointer key, gpointer value, gpointer user_data)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)user_data;
  const gchar *worker_id = (const gchar *)key;
  StatsCounterItem *counter = (StatsCounterItem *)value;
  const gchar *counter_names[] = { "queued", NULL };

  kafka_unregister_counters(self, "worker", worker_id, counter, counter_names);
}

static void
_unregister_worker_stats(KafkaSourceDriver *self)
{
  g_hash_table_foreach(self->stats_workers, _unregister_worker_counters, self);
}

static void
_register_topic_stats(KafkaSourceDriver *self)
{
  LogSourceOptions *super_options = &self->options.worker_options->super;
  gint level = super_options->stats_level;
  const gchar *counter_names[] = { "processed", NULL };

  for (int i = 0 ; i < self->assigned_partitions->cnt ; i++)
    {
      const gchar *topic = self->assigned_partitions->elems[i].topic;
      if (FALSE == g_hash_table_contains(self->stats_topics, topic))
        kafka_register_counters(self, self->stats_topics, "topic", topic, counter_names, level);
    }
}

static void
_unregister_topic_counters(gpointer key, gpointer value, gpointer user_data)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)user_data;
  const gchar *topic = (const gchar *)key;
  StatsCounterItem *counter = (StatsCounterItem *)value;
  const gchar *counter_names[] = { "processed", NULL };

  kafka_unregister_counters(self, "topic", topic, counter, counter_names);
}

static inline void
_unregister_topic_stats(KafkaSourceDriver *self)
{
  g_hash_table_foreach(self->stats_topics, _unregister_topic_counters, self);
}

static void
_register_aggregated_stats(KafkaSourceDriver *self)
{
  LogThreadedSourceWorker *worker = self->super.workers[0];
  StatsClusterKeyBuilder *kb = worker->super.metrics.stats_kb;
  stats_cluster_key_builder_push(kb);
  {
    LogSourceOptions *super_options = &self->options.worker_options->super;
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

static int64_t
_get_start_fallback_offset_code(KafkaSourceDriver *self)
{
  int64_t code = RD_KAFKA_OFFSET_END;
  if (self->options.do_not_use_bookmark)
    if (self->options.worker_options->super.read_old_records)
      code = RD_KAFKA_OFFSET_BEGINNING;
  return code;
}

static const gchar *
_get_start_fallback_offset_string(KafkaSourceDriver *self)
{
  int64_t code = _get_start_fallback_offset_code(self);
  if (code == RD_KAFKA_OFFSET_END)
    return "end";
  else
    return "beginning";
}

static gboolean
_has_wildcard_partition(GList *requested_topics)
{
  g_assert(g_list_length(requested_topics));

  for (GList *l = requested_topics; l != NULL; l = l->next)
    {
      KafkaTopicParts *item = (KafkaTopicParts *)l->data;
      for (GList *p = item->partitions; p != NULL; p = p->next)
        {
          int32_t partition = (int32_t)GPOINTER_TO_INT(p->data);
          if (partition == RD_KAFKA_PARTITION_UA)
            return TRUE;
        }
    }
  return FALSE;
}

static void
_decide_strategy(KafkaSourceDriver *self)
{
  guint topic_num = g_list_length(self->requested_topics);
  g_assert(topic_num > 0);
  KafkaTopicParts *fist_item = (KafkaTopicParts *)g_list_first(self->requested_topics)->data;
  guint fist_item_part_num = g_list_length(fist_item->partitions);

  gboolean single_topic = topic_num == 1 &&
                          FALSE == _is_topic_pattern(fist_item->topic);
  gboolean single_partition = topic_num == 1 && fist_item_part_num == 1 &&
                              (int32_t)GPOINTER_TO_INT(fist_item->partitions->data) != RD_KAFKA_PARTITION_UA;
  gboolean wildcard_partition = _has_wildcard_partition(self->requested_topics);

  if (single_topic && single_partition)
    self->startegy = KSCS_BATCH_CONSUME;
  else if (wildcard_partition)
    self->startegy = KSCS_SUBSCRIBE;
  else
    self->startegy = KSCS_ASSIGN;

  msg_verbose("kafka: selected consumer startegy",
              evt_tag_str("strategy", self->startegy == KSCS_BATCH_CONSUME ? "batch_consume" :
                          self->startegy == KSCS_SUBSCRIBE ? "subscribe_consume" : "assign_consume"),
              evt_tag_str("group_id", self->group_id),
              evt_tag_str("driver", self->super.super.super.id));
}

void
kafka_log_state_changed(KafkaSourceDriver *self, KafkaConnectedState state, rd_kafka_resp_err_t err, const char *reason)
{
  const char *state_str;
  switch(state)
    {
    case KFS_CONNECTED:
      state_str = "Connected";
      break;
    case KFS_DISCONNECTED:
      state_str = "Disconnected";
      break;
    default:
      g_assert_not_reached();
    }

  msg_verbose("kafka: Current state changed",
              evt_tag_str("state", state_str),
              evt_tag_str("group_id", self->group_id),
              evt_tag_str("driver", self->super.super.super.id));
  if (state == KFS_DISCONNECTED)
    msg_verbose("kafka: Temporally error occured",
                evt_tag_str("error", reason ? reason : rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id));
}

rd_kafka_resp_err_t
kafka_update_state(KafkaSourceDriver *self, gboolean lock)
{
  if (lock)
    kafka_opaque_state_lock(&self->opaque);

  KafkaConnectedState state = kafka_opaque_state_get(&self->opaque);

  const struct rd_kafka_metadata *metadata;
  rd_kafka_resp_err_t err = rd_kafka_metadata(self->kafka, 0, NULL, &metadata,
                                              self->options.super.state_update_timeout);
  if (err == RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      state = KFS_CONNECTED;
      kafka_opaque_state_set(&self->opaque, state);
      if (kafka_opaque_state_get_last_error(&self->opaque))
        {
          kafka_opaque_state_set_last_error(&self->opaque, 0);
          kafka_log_state_changed(self, state, err, NULL);
        }
      rd_kafka_metadata_destroy(metadata);
    }
  else
    {
      /* Though, the error can be RD_KAFKA_RESP_ERR__TIMED_OUT as well, treat it as not connected too on startup */
      if(state == KFS_UNKNOWN)
        {
          state = KFS_DISCONNECTED;
          kafka_opaque_state_set(&self->opaque, state);
          kafka_opaque_state_set_last_error(&self->opaque, err);
          kafka_log_state_changed(self, state, err, NULL);
        }
    }

  if (lock)
    kafka_opaque_state_unlock(&self->opaque);

  return err;
}

static void
_kafka_throttle_cb(rd_kafka_t *rk, const char *broker_name,
                   int32_t broker_id, int throttle_time_ms, void *opaque)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) kafka_opaque_driver((KafkaOpaque *)opaque);

  msg_info("kafka: Broker throttled request",
           evt_tag_str("group_id", self->group_id),
           evt_tag_str("broker_name", broker_name),
           evt_tag_int("throttle_time_ms", throttle_time_ms),
           evt_tag_str("driver", self->super.super.super.id));
}

void
_kafka_error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) kafka_opaque_driver((KafkaOpaque *)opaque);

  /* Do not try to detect the state (call kafka_update_state) if we should quit or if the driver is not initialized yet,
   * it seems causeing errors from the error callback triggers an infinite loop in librdkafka which
   * acceptable (?) e.g. from a consumer poll call, but prevents a correct startup/shutdown.
   * TODO: find a better way to handle this situation
   */
  if (main_loop_worker_job_quit() || (self->super.super.super.super.flags & PIF_INITIALIZED) == 0)
    return;

  kafka_opaque_state_lock(&self->opaque);

  KafkaConnectedState old_state = kafka_opaque_state_get(&self->opaque);
  if (kafka_update_state(self, FALSE) != RD_KAFKA_RESP_ERR_NO_ERROR)
    kafka_opaque_state_set(&self->opaque, KFS_DISCONNECTED);

  if (kafka_opaque_state_get(&self->opaque) == KFS_DISCONNECTED)
    {
      if(old_state != KFS_DISCONNECTED)
        kafka_log_state_changed(self, KFS_DISCONNECTED, (rd_kafka_resp_err_t) err, reason);

      if (kafka_opaque_state_get_last_error(&self->opaque) != err)
        {
          if(old_state != KFS_CONNECTED)
            {
              /* Logging further errors ony on trace level, unfortunately house keeping the last error only
               * is not enought to filter out the repetititons which would be the real goal here
               * the user can add even more detailed kafka error logging using the config options
               *      kafka-logging("kafka")
               *      config(
               *          "log_level" => "7"
               *      )
               * so, we do want to log the minimal info here in case of a connection error
               * TODO: we can try to maintain a list of already recieved errors later, and show only the new ones in this error run ?!
               */
              msg_trace("kafka: Temporally error occured",
                        //evt_tag_str("broker_name", broker_name),
                        evt_tag_str("error", reason),
                        evt_tag_str("driver", self->super.super.super.id));
            }
          kafka_opaque_state_set_last_error(&self->opaque, err);
        }
    }
  kafka_opaque_state_unlock(&self->opaque);
}

/* *********************************
 * Strategy - assign poll and queue
 * *********************************
 */
static gboolean
_setup_method_assigned_consumer(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  g_assert(topics_num > 0);

  rd_kafka_topic_partition_list_t *parts = rd_kafka_topic_partition_list_new(topics_num);
  for (GList *t = self->requested_topics; t; t = t->next)
    {
      KafkaTopicParts *tps_item = (KafkaTopicParts *)t->data;
      const gchar *requested_topic = tps_item->topic;
      GList *requested_topic_parts = tps_item->partitions;
      g_assert(g_list_length(requested_topic_parts) >= 1);

      for (GList *p = requested_topic_parts; p; p = p->next)
        {
          int32_t requested_partition = (int32_t)GPOINTER_TO_INT(p->data);
          g_assert(requested_partition != RD_KAFKA_PARTITION_UA);
          rd_kafka_topic_partition_list_add(parts, requested_topic, requested_partition);
        }
    }
  rd_kafka_resp_err_t err;
  if ((err = rd_kafka_assign(self->kafka, parts)) == RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_debug("kafka: partitions assigned",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("driver", self->super.super.super.id));
      self->assigned_partitions = parts;
      kafka_log_partition_list(parts);
      _register_topic_stats(self);
    }
  else
    {
      msg_error("kafka: rd_kafka_assign() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id));
      rd_kafka_topic_partition_list_destroy(parts);
      result = FALSE;
    }

  return result;
}

/* ************************************
 * Strategy - subscribe poll and queue
 * ************************************
 */
static gboolean
_setup_method_subscribed_consumer(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  g_assert(topics_num > 0);

  rd_kafka_topic_partition_list_t *parts = rd_kafka_topic_partition_list_new(topics_num);
  for (GList *t = self->requested_topics; t; t = t->next)
    {
      const gchar *requested_topic = ((KafkaTopicParts *)t->data)->topic;
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
      kafka_msg_debug("kafka: waiting for group rebalancer",
                      evt_tag_str("group_id", self->group_id),
                      evt_tag_str("driver", self->super.super.super.id));
      self->assigned_partitions = parts;
    }
  else
    {
      msg_error("kafka: rd_kafka_subscribe() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id));
      rd_kafka_topic_partition_list_destroy(parts);
      result = FALSE;
    }

  return result;
}

/* ***********************************************
 * Strategy - batch consume and direct processing
 * ***********************************************
 */
static gboolean
_setup_method_batch_consume(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;
  const guint topics_num = g_list_length(self->requested_topics);
  g_assert(topics_num == 1);
  KafkaTopicParts *fist_item = (KafkaTopicParts *)g_list_first(self->requested_topics)->data;
  const guint parts_num = g_list_length(fist_item->partitions);
  g_assert(parts_num == 1);

  const gchar *requested_topic = fist_item->topic;
  int32_t requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(fist_item->partitions)->data);

  rd_kafka_topic_t *new_topic = rd_kafka_topic_new(self->kafka,
                                                   requested_topic,
                                                   NULL); // TODO: add support of per-topic conf
  rd_kafka_queue_t *new_kafka_queue = rd_kafka_queue_new(self->kafka);

  if (rd_kafka_consume_start_queue(new_topic,
                                   requested_partition,
                                   _get_start_fallback_offset_code(self),
                                   new_kafka_queue) != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_error("kafka: rd_kafka_consume_start() failed",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", requested_topic),
                evt_tag_int("partition", requested_partition),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())),
                evt_tag_str("driver", self->super.super.super.id));
      if (new_kafka_queue)
        rd_kafka_queue_destroy(new_kafka_queue);
      rd_kafka_topic_destroy(new_topic);
      result = FALSE;
    }
  else
    {
      msg_verbose("kafka: batch consuming partition",
                  evt_tag_str("topic", requested_topic),
                  evt_tag_int("partition", requested_partition));
      self->consumer_kafka_queue = new_kafka_queue;
      self->topic_handle_list = g_list_append(self->topic_handle_list, new_topic);

      /* Setting up assigned partitions only for stats purpose here */
      rd_kafka_topic_partition_list_t *parts = rd_kafka_topic_partition_list_new(topics_num);
      rd_kafka_topic_partition_list_add(parts, requested_topic, requested_partition);
      self->assigned_partitions = parts;
      _register_topic_stats(self);
    }

  return result;
}

static gboolean
_adjust_num_workers(KafkaSourceDriver *self)
{
  gboolean result = TRUE;

  if (self->startegy == KSCS_BATCH_CONSUME)
    {
      if (self->super.num_workers > 2)
        {
          msg_warning("kafka: reducing worker thread count to 2 for the selected processing strategy",
                      evt_tag_str("group_id", self->group_id),
                      evt_tag_str("driver", self->super.super.super.id));
          log_threaded_source_driver_set_num_workers(&self->super.super.super, 2);
        }
    }
  else if (self->super.num_workers < 2)
    {
      msg_error("kafka: the selected processing strategy requires at least 2 worker threads",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("driver", self->super.super.super.id));
      result = FALSE;
    }
  return result;
}

static void
_alloc_msg_queues(KafkaSourceDriver *self)
{
  g_assert(self->msg_queues == NULL);
  if (self->super.num_workers - 1 <= 0)
    return;

  /* Intentionally not using the 0 index slot */
  self->used_queue_num = (self->options.separated_worker_queues ? self->super.num_workers : 2);

  self->msg_queues = g_new0(GAsyncQueue *, self->used_queue_num);
  self->queue_cond_mutexes = g_new0(GMutex, self->used_queue_num);
  self->queue_conds = g_new0(GCond, self->used_queue_num);

  /* Intentionally not using the 0 index slot */
  for (guint i = 1; i < self->used_queue_num; ++i)
    {
      self->msg_queues[i] = g_async_queue_new();
      g_mutex_init(&self->queue_cond_mutexes[i]);
      g_cond_init(&self->queue_conds[i]);
    }
}

static void
_destroy_msg_queues(KafkaSourceDriver *self)
{
  if (self->msg_queues == NULL)
    return;

  /* Intentionally not using the 0 index slot */
  for (guint i = 1; i < self->used_queue_num; ++i)
    {
      GAsyncQueue *queue = self->msg_queues[i];
      g_assert(g_async_queue_length(queue) == 0);
      g_async_queue_unref(queue);

      g_mutex_clear(&self->queue_cond_mutexes[i]);
      g_cond_clear(&self->queue_conds[i]);
    }
  g_free(self->msg_queues);
  self->msg_queues = NULL;
  g_free(self->queue_cond_mutexes);
  self->queue_cond_mutexes = NULL;
  g_free(self->queue_conds);
  self->queue_conds = NULL;
  self->used_queue_num = 0;
}

inline GAsyncQueue *
kafka_sd_worker_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker)
{
  /* Intentionally not using the 0 index slot */
  guint ndx = (self->options.separated_worker_queues ? worker->worker_index : 1);
  return self->msg_queues[ndx];
}

void
kafka_sd_wait_for_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker)
{
  /* Intentionally not using the 0 index slot */
  guint ndx = (self->options.separated_worker_queues ? worker->worker_index : 1);
  g_mutex_lock(&self->queue_cond_mutexes[ndx]);
  g_atomic_counter_inc(&self->sleeping_thread_num);
  g_cond_wait(&self->queue_conds[ndx], &self->queue_cond_mutexes[ndx]);
  g_atomic_counter_dec_and_test(&self->sleeping_thread_num);
  g_mutex_unlock(&self->queue_cond_mutexes[ndx]);
}

inline void
kafka_sd_signal_queue_ndx(KafkaSourceDriver *self, guint ndx)
{
  g_cond_signal(&self->queue_conds[ndx]);
}

inline void
kafka_sd_signal_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker)
{
  /* Intentionally not using the 0 index slot */
  guint ndx = (self->options.separated_worker_queues ? worker->worker_index : 1);
  kafka_sd_signal_queue_ndx(self, ndx);
}

inline void
kafka_sd_signal_queues(KafkaSourceDriver *self)
{
  /* Intentionally not using the 0 index slot */
  for (guint i = 1; i < self->used_queue_num; ++i)
    kafka_sd_signal_queue_ndx(self, i);
}

inline guint
kafka_sd_worker_queues_len(KafkaSourceDriver *self)
{
  guint len = 0;
  /* Intentionally not using the 0 index slot */
  for (guint i = 1; i < self->used_queue_num; ++i)
    len += g_async_queue_length(self->msg_queues[i]);
  return len;
}

void
kafka_sd_drop_queued_messages(KafkaSourceDriver *self)
{
  rd_kafka_message_t *msg;
  /* Intentionally not using the 0 index slot */
  for (guint i = 1; i < self->used_queue_num; ++i)
    {
      LogThreadedSourceWorker *target_worker = (self->options.separated_worker_queues ?
                                                self->super.workers[i] :
                                                self->super.workers[1]);
      const gchar *worker_name = kafka_src_worker_get_name(target_worker);
      GAsyncQueue *msg_queue = self->msg_queues[i];

      while ((msg = g_async_queue_try_pop(msg_queue)) != NULL)
        {
        rd_kafka_message_destroy(msg);
          kafka_sd_dec_msg_worker_stats(self, worker_name);
        }
    }
}

void
kafka_final_flush(KafkaSourceDriver *self)
{
  const gint poll_timeout = 50;

  gint remaining_time = self->options.super.poll_timeout;
  while (rd_kafka_outq_len(self->kafka) > 0 && remaining_time > 0)
    {
      rd_kafka_poll(self->kafka, poll_timeout);
      remaining_time -= poll_timeout;
    }

  if (rd_kafka_outq_len(self->kafka) > 0)
    msg_warning("kafka: final outq flush could not process all items",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("driver", self->super.super.super.id));
}

void
kafka_sd_wakeup_kafka_queues(KafkaSourceDriver *self)
{
  if (self->consumer_kafka_queue)
    rd_kafka_queue_yield(self->consumer_kafka_queue);
  if (self->main_kafka_queue)
    rd_kafka_queue_yield(self->main_kafka_queue);
}

static void
_kafka_rebalance_cb(rd_kafka_t *rk,
                    rd_kafka_resp_err_t err,
                    rd_kafka_topic_partition_list_t *partitions,
                    void *opaque)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) kafka_opaque_driver((KafkaOpaque *)opaque);

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
      _register_topic_stats(self);
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
_g_int32_equal(gconstpointer  v1, gconstpointer  v2)
{
  return (int32_t)GPOINTER_TO_INT(v1) == (int32_t)GPOINTER_TO_INT(v2);
}

static gboolean
_g_int32_compare(gconstpointer  v1, gconstpointer  v2)
{
  if ((int32_t)GPOINTER_TO_INT(v1) < (int32_t)GPOINTER_TO_INT(v2))
    return -1;
  else if ((int32_t)GPOINTER_TO_INT(v1) > (int32_t)GPOINTER_TO_INT(v2))
    return 1;
  else
    return 0;
}

static gboolean
_check_and_sort_partitions(KafkaSourceDriver *self, const gchar *partitions, GList **requested_partitions)
{
  GList *list = split_and_convert_to_list("partition", partitions, ",", TRUE, NULL,
                                          _convert_to_int, _validate_part_num);
  if (list == NULL)
    return FALSE;

  list = g_list_sort(list, _g_int32_compare);
  const gint original_length = g_list_length(list);
  if (list)
    list = g_list_remove_duplicates(list, _g_int32_equal, NULL);

  const gpointer all_parts = GINT_TO_POINTER((gint)RD_KAFKA_PARTITION_UA);
  if (g_list_find(list, all_parts))
    {
      if (g_list_length(list) > 1)
        {
          msg_error("kafka: error, 'all partitions' (-1) specified along with other partition numbers",
                    evt_tag_str("partitions", partitions));
          return FALSE;
        }
    }

  if (requested_partitions)
    {
      if (original_length != g_list_length(list))
        msg_warning("kafka: dropped duplicated entries of partition numbers",
                    evt_tag_str("partitions", partitions));
      *requested_partitions = list;
    }
  return TRUE;
}

static gboolean
_check_and_apply_topics(KafkaSourceDriver *self, GList *topics, gboolean apply)
{
  if (g_list_length(topics) == 0)
    return FALSE;

  GList *requested_topics = NULL;
  for (GList *topic_option = topics; topic_option != NULL; topic_option = topic_option->next)
    {
      GList *requested_partitions = NULL;
      KafkaProperty *prop = (KafkaProperty *)topic_option->data;

      if (_check_and_sort_partitions(self, (const gchar *) prop->value, &requested_partitions))
        {
          GError *err = NULL;
          if (_validate_topic_name(prop->name, &err))
            {
              KafkaTopicParts *new_item = kafka_tps_new(prop->name, requested_partitions);
              requested_topics = g_list_prepend(requested_topics, new_item);
              continue;
            }
          else
            {
              msg_error("kafka: invalid topic name in the requested topics list",
                        evt_tag_str("topic", prop->name),
                        evt_tag_str("error", err->message));
              if (err)
                g_error_free(err);
            }
        }
      if (requested_partitions)
        g_list_free(requested_partitions);
      if (requested_topics)
        kafka_tps_list_free(requested_topics);
      return FALSE;
    }
  g_assert(requested_topics != NULL);

  const gint original_length = g_list_length(requested_topics);
  requested_topics = g_list_remove_duplicates(requested_topics, kafka_tps_equal, (GDestroyNotify) kafka_tps_free);

  if (apply)
    {
      if (original_length != g_list_length(requested_topics))
        msg_warning("kafka: dropped duplicated entries of topics config option");
      if (self->requested_topics)
        kafka_tps_list_free(self->requested_topics);
      self->requested_topics = requested_topics;
    }
  else if (requested_topics)
    kafka_tps_list_free(requested_topics);

  return TRUE;
}

static void
_apply_group_id(KafkaSourceDriver *self)
{
  const gchar *group_id_key = "group.id";
  gchar *group_id = NULL;
  const gchar *conf_group_id = kafka_property_list_find_not_empty(self->options.super.config, group_id_key);

  if (conf_group_id )
        {
      group_id = g_strdup(conf_group_id);
      kafka_msg_debug("kafka: found config group.id",
                      evt_tag_str("group_id", group_id),
                      evt_tag_str("driver", self->super.super.super.id));
        }
      else
        {
      group_id = g_strdup(self->super.super.super.id);
      KafkaProperty *kp_groupid = g_new0(KafkaProperty, 1);
      kp_groupid->name = g_strdup(group_id_key);
      kp_groupid->value = g_strdup(group_id);
      self->options.super.config = g_list_prepend(self->options.super.config, kp_groupid);
      kafka_msg_debug("kafka: config group.id not found, using a self-generated one",
                      evt_tag_str("group_id", group_id),
                      evt_tag_str("driver", group_id));
    }

  g_free(self->group_id);
  self->group_id = group_id;
}

static void
_apply_options(KafkaSourceDriver *self)
{
  _apply_group_id(self);
  _check_and_apply_topics(self, self->options.requested_topics, TRUE);
}

static rd_kafka_t *
_construct_kafka_client(KafkaSourceDriver *self)
{
  kafka_msg_debug("kafka: constructing client",
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("driver", self->super.super.super.id));

  rd_kafka_conf_t *conf = rd_kafka_conf_new();

  if (FALSE == kafka_conf_set_prop(conf, "metadata.broker.list", self->options.super.bootstrap_servers))
    goto err_exit;
  /* NOTE: The callback-based consumer API's offset store granularity is
   * said to be not good enough, also, we want to control the offset store
   * process, and treat the message processed if we were able to fuly deliver it.
   * Disable the automatic offset store, do it explicitly per-message in
   * the message processor of the consume callback/batch_loop instead.
   * Also, this controls only the high-level consumer API scenario, for the low-lewel
   * version we should use our persist state backup/restore and RD_KAFKA_OFFSET_STORED
   * in rd_kafka_consume_start */
  if (FALSE == kafka_conf_set_prop(conf, "enable.auto.offset.store", "false"))
    goto err_exit;
  if (FALSE == kafka_conf_set_prop(conf, "enable.auto.commit", "true"))
    goto err_exit;
  /* Like RD_KAFKA_OFFSET_XXX of rd_kafka_consume_start for the high-level consumer API
   * this is just a fallback if the offset cannot be restored.
   * NOTE: this is treated a per-topic option in the documentation, but seems to apply globally.
   */
  if (FALSE == kafka_conf_set_prop(conf, "auto.offset.reset", _get_start_fallback_offset_string(self)))
    goto err_exit;

  static gchar *protected_properties[] =
  {
    "bootstrap.servers",
    "metadata.broker.list",
    "enable.auto.offset.store",
    "auto.offset.reset",
    "enable.auto.commit",
    "auto.commit.enable",
  };
  if (FALSE == kafka_apply_config_props(conf, self->options.super.config, protected_properties,
                                        G_N_ELEMENTS(protected_properties)))
    goto err_exit;

  rd_kafka_conf_set_opaque(conf, &self->opaque);
  if (self->options.super.kafka_logging != KFL_DISABLED)
    rd_kafka_conf_set_log_cb(conf, kafka_log_callback);
  rd_kafka_conf_set_error_cb(conf, _kafka_error_cb);
  rd_kafka_conf_set_throttle_cb(conf, _kafka_throttle_cb);
  rd_kafka_conf_set_rebalance_cb(conf, _kafka_rebalance_cb);

  gchar errbuf[1024] = {0};
  rd_kafka_t *client = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errbuf, sizeof(errbuf));
  if (NULL == client)
    {
      msg_error("kafka: error constructing the kafka connection object",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("error", errbuf),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
  return client;

err_exit:
  rd_kafka_conf_destroy(conf);
  return NULL;
}

static gboolean
_setup_kafka_client(KafkaSourceDriver *self)
{
  g_assert(self->kafka);
  gboolean result = TRUE;

  switch(self->startegy)
    {
    case KSCS_ASSIGN:
      result = _setup_method_assigned_consumer(self);
      break;
    case KSCS_SUBSCRIBE:
      result = _setup_method_subscribed_consumer(self);
      break;
    case KSCS_BATCH_CONSUME:
      result = _setup_method_batch_consume(self);
      break;
    default:
      g_assert_not_reached();
    }
  rd_kafka_poll(self->kafka, 0);
  return result;
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

  kafka_msg_debug("kafka: destroying client",
                  evt_tag_str("group_id", self->group_id),
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

  if (self->kafka)
    {
      if (self->startegy == KSCS_BATCH_CONSUME)
        {
          if (self->consumer_kafka_queue)
            rd_kafka_queue_destroy(self->consumer_kafka_queue);
          self->consumer_kafka_queue = NULL;
        }
      else
        {
          /* These are just borrowed temporally in _consumer_run_consumer_poll */
          g_assert(self->consumer_kafka_queue == NULL && self->main_kafka_queue == NULL);

          if (self->startegy == KSCS_SUBSCRIBE)
            rd_kafka_unsubscribe(self->kafka);
          else
            rd_kafka_assign(self->kafka, NULL);
        }
      kafka_final_flush(self);

      rd_kafka_destroy(self->kafka);
      self->kafka = NULL;
    }

  if (self->options.super.config)
    kafka_property_list_free(self->options.super.config);
  self->options.super.config = NULL;

  g_free(self->group_id);
  self->group_id = NULL;

  if (self->requested_topics)
    kafka_tps_list_free(self->requested_topics);
  self->requested_topics = NULL;

  kafka_opaque_deinit(&self->opaque);
  _destroy_msg_queues(self);
}

gboolean
kafka_sd_reopen(LogDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  if (self->kafka)
    _destroy_kafka_client(s);

  kafka_opaque_init(&self->opaque, &self->super.super.super, &self->options.super);

  self->kafka = _construct_kafka_client(self);
  if (self->kafka == NULL)
    {
      msg_error("kafka: error constructing kafka connection object",
                evt_tag_str("group_id", self->group_id),
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

  /* Order is important here
   *  - apply options first to have group.id set correctly
   *  - then decide strategy based on options, as it may affect worker and used queue count
   *  - then allocate message queues based on strategy and worker count
   *  - then allocate the stats hash tables as opening the kafka connection may register topic stats
   *  - then call the parent init to setup threading
   *  - then open the kafka connection which requires to be ready the group.id and the threading
   *  - registering further stats need threading and group.id set correctly too
   */
  _apply_options(self);
  _decide_strategy(self);

  if (FALSE == _adjust_num_workers(self))
    return FALSE;

  _alloc_msg_queues(self);
  self->stats_topics = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  self->stats_workers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  if (FALSE == log_threaded_source_driver_init_method(s))
    return FALSE;

  if (FALSE == kafka_sd_reopen(&self->super.super.super))
    return FALSE;

  _register_worker_stats(self);
  _register_aggregated_stats(self);

  GlobalConfig *cfg = log_pipe_get_config(s);
  log_template_options_init(&self->options.template_options, cfg);
  msg_verbose("kafka: Kafka source initialized",
              evt_tag_str("group_id", self->group_id),
              evt_tag_str("driver", self->super.super.super.id),
              log_pipe_location_tag(&self->super.super.super.super));
  return TRUE;
}

static gboolean
kafka_sd_deinit(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  _destroy_kafka_client(&self->super.super.super);

  _unregister_aggregated_stats(self);
  _unregister_worker_stats(self);
  _unregister_topic_stats(self);
  g_hash_table_destroy(self->stats_topics);
  g_hash_table_destroy(self->stats_workers);

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

  /* The default number of workers is best to set to a minimum of 2
   * to allow parallelism between fetching and processing messages,
   * even for the single_topic/single_partition KSCS_BATCH_CONSUME processing strategy.
   * User can override it later if needed, but can be reduced to 1 only if the processing
   * strategy allows it (KSCS_BATCH_CONSUME).
   */
  self->super.num_workers = 2;

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
  self->super.worker_construct = kafka_src_worker_new;

  self->super.format_stats_key = _format_stats_key;

  kafka_sd_options_defaults(&self->options, &self->super.worker_options);

  return &self->super.super.super;
}

/* *********
 *  Options
 * *********/

void
kafka_sd_merge_config(LogDriver *d, GList *props)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_merge_config(&self->options.super, props);
}

gboolean
kafka_sd_set_logging(LogDriver *d, const gchar *logging)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  return kafka_options_set_logging(&self->options.super, logging);
}

void
kafka_sd_set_bootstrap_servers(LogDriver *d, const gchar *bootstrap_servers)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_set_bootstrap_servers(&self->options.super, bootstrap_servers);
}

gboolean
kafka_sd_set_topics(LogDriver *d, GList *topics)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) d;

  if (self->options.requested_topics)
    kafka_property_list_free(self->options.requested_topics);
  self->options.requested_topics = topics;

  return _check_and_apply_topics(self, topics, FALSE);
}

void
kafka_sd_set_poll_timeout(LogDriver *d, gint poll_timeout)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_set_poll_timeout(&self->options.super, poll_timeout);
}

void
kafka_sd_set_state_update_timeout(LogDriver *d, gint state_update_timeout)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)d;

  kafka_options_set_state_update_timeout(&self->options.super, state_update_timeout);
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
kafka_sd_set_log_fetch_limit(LogDriver *s, guint new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.fetch_limit = new_value;
}

void
kafka_sd_set_log_fetch_queue_full_delay(LogDriver *s, guint new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.fetch_queue_full_delay = new_value;
}

void kafka_sd_set_single_worker_queue(LogDriver *s, gboolean new_value)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;
  self->options.separated_worker_queues = (FALSE == new_value);
}

void
kafka_sd_options_defaults(KafkaSourceOptions *self,
                          LogThreadedSourceWorkerOptions *worker_options)
{
  self->worker_options = worker_options;
  self->worker_options->super.stats_level = STATS_LEVEL0;
  self->worker_options->super.stats_source = stats_register_type("kafka");

  /* No additional format options now, so intentionally referencing only, and not using msg_format_options_copy */
  self->format_options = &self->worker_options->parse_options;

  kafka_options_defaults(&self->super);
  self->time_reopen = 60;

  self->do_not_use_bookmark = FALSE;
  self->fetch_delay = 10000; /* 1 second / 10000 */
  self->fetch_limit = 1000;
  self->fetch_queue_full_delay = 1000; /* 1 second */
  self->separated_worker_queues = FALSE;

  log_template_options_defaults(&self->template_options);
}

void
kafka_sd_options_destroy(KafkaSourceOptions *self)
{
  kafka_property_list_free(self->requested_topics);
  log_template_unref(self->message);
  self->message = NULL;

  kafka_options_destroy(&self->super);
  log_template_options_destroy(&self->template_options);

  /* Just referenced, not copied */
  self->format_options = NULL;
  self->worker_options = NULL;
}
