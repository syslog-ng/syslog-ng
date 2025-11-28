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

#include "kafka-source-worker.h"
#include "kafka-source-persist.h"
#include "kafka-internal.h"
#include "kafka-props.h"
#include "kafka-topic-parts.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/aggregator/stats-aggregator.h"

static gsize
_log_message_from_string(const char *msg_cstring, MsgFormatOptions *format_options, LogMessage **out_msg)
{
  gsize msg_len = strlen(msg_cstring);
  *out_msg = msg_format_construct_message(format_options, (const guchar *) msg_cstring, msg_len);
  msg_format_parse_into(format_options, *out_msg, (const guchar *) msg_cstring, msg_len);
  return msg_len;
}

gboolean _has_wildcard_partition(GList *requested_topics)
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

static inline void
_send(LogThreadedSourceWorker *self, LogMessage *msg)
{
  log_threaded_source_worker_blocking_post(self, msg);
}

static inline gdouble
_mainloop_sleep_time(const gdouble delay)
{
  return delay > 0 ? 1.0 / delay : 0.0;
}

static gboolean
_process_message(LogThreadedSourceWorker *worker, rd_kafka_message_t *msg)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) worker->control;
  KafkaSourceWorker *self = (KafkaSourceWorker *) worker;
  const gchar *topic_name = rd_kafka_topic_name(msg->rkt);

  msg_trace("kafka: processing message",
            evt_tag_str("group_id", driver->group_id),
            evt_tag_str("topic", topic_name),
            evt_tag_int("partition", msg->partition),
            evt_tag_str("message", (gchar *)msg->payload),
            evt_tag_long("offset", msg->offset),
            evt_tag_str("driver", driver->super.super.super.id));

  LogMessage *log_msg;
  gsize msg_len = _log_message_from_string((gchar *)msg->payload, driver->options.format_options, &log_msg);
  log_msg_set_value_to_string(log_msg, LM_V_TRANSPORT, "local+kafka");

  if (FALSE == driver->options.disable_bookmarks)
    kafka_sd_persist_store_msg_offset(driver, self->super.super.ack_tracker, topic_name, msg->partition, msg->offset);

  _send(worker, log_msg);

  kafka_sd_update_msg_length_stats(driver, msg_len);
  log_msg_set_recvd_rawmsg_size(log_msg, msg->len);
  kafka_sd_inc_msg_topic_stats(driver, rd_kafka_topic_name(msg->rkt));

  rd_kafka_message_destroy(msg);

  main_loop_worker_run_gc();

  return TRUE;
}

static inline ThreadedFetchResult
_fetch(LogThreadedSourceWorker *self, rd_kafka_message_t **msg)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->control;
  GAsyncQueue *msg_queue = kafka_sd_worker_queue(driver, self);

  // TODO: Add if batched_ack_tracker_factory_new support is added
  // if (G_UNLIKELY(self->curr_fetch_in_run >= self->options.fetch_limit))
  //   {
  //   log_threaded_source_worker_close_batch(self);
  //   return THREADED_FETCH_TRY_AGAIN
  //   }

  *msg = g_async_queue_try_pop(msg_queue);
  if (G_LIKELY(*msg != NULL))
    {
      kafka_sd_update_msg_worker_stats(driver, self->worker_index);
      return THREADED_FETCH_SUCCESS;
    }
  return THREADED_FETCH_NO_DATA;
}

/* runs in a dedicated thread */
static void
_processor_run(LogThreadedSourceWorker *self)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->control;

  g_atomic_counter_inc(&driver->running_thread_num);

  kafka_msg_debug("kafka: started queue processor",
                  evt_tag_int("index", self->worker_index),
                  evt_tag_str("group_id", driver->group_id),
                  evt_tag_str("driver", driver->super.super.super.id));

  GAsyncQueue *msg_queue = kafka_sd_worker_queue(driver, self);
  const gdouble iteration_sleep_time = _mainloop_sleep_time(driver->options.fetch_delay);
  const gdouble fetch_retry_sleep_time = _mainloop_sleep_time(driver->options.fetch_retry_delay);
  rd_kafka_message_t *msg = NULL;

  while (1)
    {
      if (G_UNLIKELY(main_loop_worker_job_quit()))
        break;

      /* Lazy check for empty queue */
      if (g_async_queue_length(msg_queue) == 0)
        kafka_sd_wait_for_queue(driver, self);

      while (1)
        {
          if (G_UNLIKELY(main_loop_worker_job_quit()))
            break;

          if (G_LIKELY(_fetch(self, &msg) == THREADED_FETCH_SUCCESS))
            {
              _process_message(self, msg);
              rd_kafka_poll(driver->kafka, 0);
              main_loop_worker_wait_for_exit_until(iteration_sleep_time);
              continue;
            }

          main_loop_worker_wait_for_exit_until(fetch_retry_sleep_time);
          break;
        }
    }
  kafka_msg_debug("kafka: stopped queue processor",
                  evt_tag_int("index", self->worker_index),
                  evt_tag_str("group_id", driver->group_id),
                  evt_tag_str("driver", driver->super.super.super.id),
                  evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(driver->kafka)),
                  evt_tag_int("worker_queue_len", g_async_queue_length(msg_queue)));
  g_atomic_counter_dec_and_test(&driver->running_thread_num);
}

static gboolean
_queue_message(KafkaSourceWorker *self, rd_kafka_message_t *msg, guint target_queue_ndx)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->super.control;

  msg_trace("kafka: queuing message",
            evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
            evt_tag_int("partition", msg->partition),
            evt_tag_int("offset", msg->offset),
            evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(driver->kafka)),
            evt_tag_int("worker_queues_len", kafka_sd_worker_queues_len(driver)));
  do
    {
      gint msg_queues_len = kafka_sd_worker_queues_len(driver);

      if (G_LIKELY(msg_queues_len < driver->options.fetch_limit))
        break;

      kafka_msg_debug("kafka: message queue full, waiting",
                      evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(driver->kafka)),
                      evt_tag_int("worker_queues_len", msg_queues_len));
      kafka_sd_signal_queues(driver);
      rd_kafka_poll(driver->kafka, driver->options.fetch_queue_full_delay);

      if (G_UNLIKELY(main_loop_worker_job_quit()))
        return FALSE;
    }
  while (1);

  g_async_queue_push(driver->msg_queues[target_queue_ndx], msg);
  kafka_sd_signal_queue_ndx(driver, target_queue_ndx);

  kafka_sd_update_msg_worker_stats(driver, target_queue_ndx);

  return TRUE;
}

static inline guint
_next_target_queue_ndx(KafkaSourceDriver *driver, guint *rr)
{
  guint target_queue_ndx = (driver->options.separated_worker_queues ?
                            ((*rr)++ % (driver->super.num_workers - 1)) + 1 :
                            1); /* Intentionally not using the 0 index slot */
  return target_queue_ndx;
}

static void
_wait_for_queue_processors_to_exit(KafkaSourceDriver *driver, const gdouble iteration_sleep_time)
{
  /* Wake-up sleeping/waiting (queue) processor workers */
  kafka_sd_signal_queues(driver);
  /* Wait for all the (queue) processor workers to exit */
  while (g_atomic_counter_get(&driver->running_thread_num) > 1)
    {
      main_loop_worker_wait_for_exit_until(iteration_sleep_time);
      kafka_sd_signal_queues(driver);
    }
}

static void
_stop_consumer(KafkaSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) worker->super.control;

  /* This call will block until the consumer has revoked its assignment, calling the rebalance_cb if it is configured,
   * committed offsets to broker, and left the consumer group (if applicable).
   */
  rd_kafka_consumer_close(driver->kafka);

  if (driver->strategy == KSCS_SUBSCRIBE)
    rd_kafka_unsubscribe(driver->kafka);
  else
    rd_kafka_assign(driver->kafka, NULL);

  /* Wait for outstanding requests to finish */
  kafka_final_flush(driver);
}

/* runs in a dedicated thread */
static void
_run_consumer(KafkaSourceWorker *self, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->super.control;
  gboolean persist_use_offset_tracker = kafka_sd_parallel_processing(driver);
  gboolean all_persists_ready = (FALSE == persist_use_offset_tracker);
  rd_kafka_message_t *msg;
  guint rr = 0;

  kafka_msg_debug("kafka: consumer poll run started",
                  evt_tag_int("worker_num", driver->super.num_workers),
                  evt_tag_int("queue_num", driver->allocated_queue_num ? driver->allocated_queue_num - 1 : 0),
                  evt_tag_int("queues_max_size", driver->options.fetch_limit),
                  evt_tag_str("persist_store", driver->options.persist_store == KSPS_LOCAL ? "local" : "remote"),
                  evt_tag_str("group_id", driver->group_id),
                  evt_tag_str("driver", driver->super.super.super.id));
  if (driver->strategy == KSCS_SUBSCRIBE)
    kafka_msg_debug("kafka: waiting for group rebalancer",
                    evt_tag_str("group_id", driver->group_id),
                    evt_tag_str("driver", driver->super.super.super.id));
  while (1)
    {
      if (G_UNLIKELY(main_loop_worker_job_quit()))
        break;

      rd_kafka_poll(driver->kafka, 0);
      msg = rd_kafka_consumer_poll(driver->kafka, driver->options.super.poll_timeout);

      if (msg == NULL || msg->err)
        {
          kafka_update_state(driver, TRUE);

          if (msg == NULL || msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
            {
              if (kafka_opaque_state_get(&driver->opaque) != KFS_CONNECTED)
                {
                  main_loop_worker_wait_for_exit_until(driver->options.time_reopen);
                  kafka_update_state(driver, TRUE);
                  continue;
                }
              kafka_msg_debug("kafka: consumer_poll - no data",
                              evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(driver->kafka)),
                              evt_tag_int("worker_queues_len", kafka_sd_worker_queues_len(driver)));
              if (msg)
                rd_kafka_message_destroy(msg);
            }
          else
            {
              msg_error("kafka: consumer poll message error",
                        evt_tag_str("group_id", driver->group_id),
                        evt_tag_str("topic", msg && msg->rkt ? rd_kafka_topic_name(msg->rkt) : ""),
                        evt_tag_int("partition", msg ? msg->partition : RD_KAFKA_PARTITION_UA),
                        evt_tag_str("error", rd_kafka_err2str(msg->err)),
                        evt_tag_str("driver", driver->super.super.super.id));
              rd_kafka_message_destroy(msg);
              break;
            }
        }
      else
        {
          gboolean can_use_queue = driver->allocated_queue_num > 0;
          /* This is needed for now because offset tracker readiness for topics which has no stored offsets yet
           * is only guaranteed after the first message is consumed for a given topic-partition
           * (see kafka_source_persist_is_ready implementation and offset_tracker_new comments for details)
           * TODO: improve offset tracker readiness handling to avoid this check per message
           */
          if (G_UNLIKELY(can_use_queue && FALSE == all_persists_ready))
            {
              gboolean persist_is_ready = kafka_sd_persist_is_ready(driver, rd_kafka_topic_name(msg->rkt), msg->partition,
                                                                    &all_persists_ready);
              if (FALSE == persist_is_ready)
                can_use_queue = FALSE;
            }

          if (can_use_queue)
            {
              if (G_UNLIKELY(FALSE == _queue_message(self, msg, _next_target_queue_ndx(driver, &rr))))
                rd_kafka_message_destroy(msg);
            }
          else
            {
              _process_message(&self->super, msg);
              rd_kafka_poll(driver->kafka, 0);
              main_loop_worker_wait_for_exit_until(iteration_sleep_time);
            }
        }
    }
  _wait_for_queue_processors_to_exit(driver, iteration_sleep_time);
}

static gboolean
_restart_consumer(KafkaSourceWorker *self, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->super.control;

  main_loop_worker_wait_for_exit_until(driver->options.time_reopen);

  while (FALSE == main_loop_worker_job_quit())
    {
      kafka_update_state(driver, TRUE);
      if (g_atomic_counter_get(&driver->sleeping_thread_num) == driver->super.num_workers - 1)
        {
          kafka_sd_reopen(&driver->super.super.super);
          break;
        }
      msg_trace("kafka: waiting for queue processors to sleep",
                evt_tag_str("group_id", driver->group_id),
                evt_tag_str("driver", driver->super.super.super.id));
      main_loop_worker_wait_for_exit_until(iteration_sleep_time);
    }

  return FALSE == main_loop_worker_job_quit();
}

/* runs in a dedicated thread */
static void
_consumer_run(LogThreadedSourceWorker *worker)
{
  KafkaSourceWorker *self = (KafkaSourceWorker *) worker;
  KafkaSourceDriver *driver = (KafkaSourceDriver *) self->super.control;
  const gdouble iteration_sleep_time = _mainloop_sleep_time(driver->options.fetch_delay);
  gboolean exit_requested = FALSE;

  g_atomic_counter_inc(&driver->running_thread_num);
  do
    {
      /* We just steal these from the main/consumer event loops to be able to stop the blocking rd_kafka_consumer_poll */
      driver->consumer_kafka_queue = rd_kafka_queue_get_consumer(driver->kafka);
      driver->main_kafka_queue = rd_kafka_queue_get_main(driver->kafka);

      /* The order and the conditonal entering to the _run_consumer loop is very important here now for the assign strategy
       * see _kafka_error_cb for details */
      kafka_update_state(driver, TRUE);
      rd_kafka_poll(driver->kafka, 0);

      if (kafka_opaque_state_get(&driver->opaque) == KFS_CONNECTED ||
          driver->strategy == KSCS_SUBSCRIBE) // see _kafka_error_cb for details
        {
          _run_consumer(self, iteration_sleep_time);
          _stop_consumer(self, iteration_sleep_time);

          kafka_msg_debug("kafka: stopped consumer poll run",
                          evt_tag_str("group_id", driver->group_id),
                          evt_tag_str("driver", driver->super.super.super.id));
        }

      rd_kafka_queue_destroy(driver->consumer_kafka_queue);
      driver->consumer_kafka_queue = NULL;
      rd_kafka_queue_destroy(driver->main_kafka_queue);
      driver->main_kafka_queue = NULL;

      main_loop_worker_run_gc();
      exit_requested = main_loop_worker_job_quit();
      if (FALSE == exit_requested)
        {
          /* Getting here means an error happened in the consumer, let's retry from ground */
          exit_requested = (FALSE == _restart_consumer(self, iteration_sleep_time));
        }
    }
  while (FALSE == exit_requested);

  _wait_for_queue_processors_to_exit(driver, iteration_sleep_time);
  if (driver->allocated_queue_num > 0)
    kafka_sd_drop_queued_messages(driver);

  gboolean all_threads_exited = g_atomic_counter_dec_and_test(&driver->running_thread_num);
  g_assert(all_threads_exited);
}

static void
_exit_requested(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *driver = (KafkaSourceDriver *) worker->control;
  /* We need to wake-up the possibly blocking rd_kafka_consumer_poll/rd_kafka_poll */
  kafka_sd_wakeup_kafka_queues(driver);
}

static gboolean
_kafka_src_worker_init(LogThreadedSourceWorker *worker,
                       /* cannot use worker->control, not yet set */
                       LogThreadedSourceDriver *owner)
{
  KafkaSourceWorker *self = (KafkaSourceWorker *) worker;

  int ret = g_snprintf(self->name, sizeof(self->name), "worker#%d", self->super.worker_index);
  g_assert((gsize)ret < sizeof(self->name));

  if (self->super.worker_index == 0)
    {
      self->super.run = _consumer_run;
      self->super.request_exit = _exit_requested;
    }
  else
    self->super.run = _processor_run;

  return TRUE;
}

inline const gchar *
kafka_src_worker_get_name(LogThreadedSourceWorker *worker)
{
  KafkaSourceWorker *self = (KafkaSourceWorker *) worker;
  return self->name;
}

LogThreadedSourceWorker *kafka_src_worker_new(LogThreadedSourceDriver *owner, gint worker_index)
{
  KafkaSourceWorker *self = g_new0(KafkaSourceWorker, 1);

  log_threaded_source_worker_init_instance(&self->super, owner, worker_index);

  /* NOTE: Cannot use self->super.thread_init, as kafka_src_worker_get_name might be called before thread_init is called
   *       also, name cannot be a dynamically allocated GString, as it might be used during shutdown/cleanups etc.,
   *       like the worker_index, and currently there is no LogThreadedSourceDriver thread.free hook to free such resources.
   *       It also means that the control pointer still not set when thread_init is called, we must pass the owner explicitly.
   */
  _kafka_src_worker_init(&self->super, owner);

  return &self->super;
}
