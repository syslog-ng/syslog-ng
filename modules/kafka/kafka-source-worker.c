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
#include "kafka-internal.h"
#include "kafka-props.h"
#include "kafka-topic-parts.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/aggregator/stats-aggregator.h"

static void
_log_reader_insert_msg_length_stats(KafkaSourceDriver *self, gsize len)
{
  stats_aggregator_add_data_point(self->max_message_size, len);
  stats_aggregator_add_data_point(self->average_messages_size, len);
}

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

  if (self->startegy == KSCS_SUBSCRIBE_POLL_QUEUED || self->startegy == KSCS_ASSIGN_POLL_QUEUED)
    {
      rd_kafka_error_t *err = rd_kafka_offset_store_message(msg);
      if (err)
        msg_warning("kafka: error storing message offset",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                    evt_tag_int("partition", msg->partition),
                    evt_tag_str("error", rd_kafka_error_string(err)),
                    evt_tag_str("driver", self->super.super.super.id));
    }
  rd_kafka_message_destroy(msg);
  main_loop_worker_run_gc();

  return TRUE;
}

static inline ThreadedFetchResult
_fetch(LogThreadedSourceWorker *worker, rd_kafka_message_t **msg)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  GAsyncQueue *msg_queue = kafka_sd_worker_queue(self, worker);
  do
    {
      if ((*msg = g_async_queue_try_pop(msg_queue)))
        return THREADED_FETCH_SUCCESS;
    }
  while (g_async_queue_length(msg_queue) && FALSE == main_loop_worker_job_quit());
  return THREADED_FETCH_NO_DATA;
}

/* runs in a dedicated thread */
static void
_processor_run(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  g_atomic_counter_inc(&self->running_thread_num);

  msg_debug("kafka: started queue processor",
            evt_tag_int("index", worker->worker_index),
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("driver", self->super.super.super.id));

  GAsyncQueue *msg_queue = kafka_sd_worker_queue(self, worker);
  const gdouble iteration_sleep_time = _iteration_sleep_time(worker);
  rd_kafka_message_t *msg = NULL;

  while (FALSE == main_loop_worker_job_quit())
    {
      if (g_async_queue_length(msg_queue) == 0)
        kafka_sd_wait_for_queue(self, worker);

      while (FALSE == main_loop_worker_job_quit())
        {
          ThreadedFetchResult fetch_result = _fetch(worker, &msg);
          if (fetch_result == THREADED_FETCH_SUCCESS)
            {
              _process_message(worker, msg);
              rd_kafka_poll(self->kafka, 0);
              main_loop_worker_wait_for_exit_until(iteration_sleep_time);
            }
          else if (fetch_result == THREADED_FETCH_NO_DATA)
            break;
        }
    }
  msg_debug("kafka: stopped queue processor",
            evt_tag_int("index", worker->worker_index),
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("driver", self->super.super.super.id));

  g_atomic_counter_dec_and_test(&self->running_thread_num);
}

/* ****************************************************
 * Strategy - assign/subscribe consumer poll and queue
 * ****************************************************/

/* runs in a dedicated thread */
static void
_consumer_run_consumer_poll(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  rd_kafka_message_t *msg;
  guint rr = 0;

  msg_debug("kafka: consumer poll run started - queued",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("driver", self->super.super.super.id));

  /* We just steal these from the main/consumer event loops to be able to stop the blocking rd_kafka_consumer_poll */
  self->consumer_kafka_queue = rd_kafka_queue_get_consumer(self->kafka);
  self->main_kafka_queue = rd_kafka_queue_get_main(self->kafka);

  while (FALSE == main_loop_worker_job_quit())
    {
      gint msg_queue_len = kafka_sd_worker_queues_len(self);
      if (msg_queue_len >= self->options.fetch_limit)
        {
          msg_verbose("kafka: message queue full, waiting",
                      evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                      evt_tag_int("msg_queue_len", msg_queue_len));
          kafka_sd_signal_queues(self);
          rd_kafka_poll(self->kafka, self->options.fetch_queue_full_delay);
          continue;
        }

      rd_kafka_poll(self->kafka, 0);
      msg = rd_kafka_consumer_poll(self->kafka, self->options.super.poll_timeout);
      if (msg == NULL || msg->err)
        {
          kafka_update_state(self, TRUE);
          if (msg == NULL || msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
            {
              msg_debug("kafka: consumer_poll - no data",
                        evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                        evt_tag_int("msg_queue_len", kafka_sd_worker_queues_len(self)));
              if (msg)
                rd_kafka_message_destroy(msg);
            }
          else
            {
              msg_error("kafka: consumer poll message error",
                        evt_tag_str("group_id", self->group_id),
                        evt_tag_str("topic", msg && msg->rkt ? rd_kafka_topic_name(msg->rkt) : ""),
                        evt_tag_int("partition", msg ? msg->partition : RD_KAFKA_PARTITION_UA),
                        evt_tag_str("error", rd_kafka_err2str(msg->err)),
                        evt_tag_str("driver", self->super.super.super.id));
              rd_kafka_message_destroy(msg);
              break;
            }
        }
      else
        {
          msg_trace("kafka: got message",
                    evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                    evt_tag_int("partition", msg->partition),
                    evt_tag_int("offset", msg->offset),
                    evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                    evt_tag_int("msg_queue_len", kafka_sd_worker_queues_len(self)));

          guint target_queue = (self->options.separated_worker_queues ?
                                (rr++ % (self->super.num_workers - 1)) + 1 :
                                1); /* Intentionally not using the 0 index slot */
          g_async_queue_push(self->msg_queues[target_queue], msg);
          kafka_sd_signal_queue_ndx(self, target_queue);
        }
    }
  /* This call will block until the consumer has revoked its assignment, calling the rebalance_cb if it is configured,
   * committed offsets to broker, and left the consumer group (if applicable).
   */
  rd_kafka_consumer_close(self->kafka);
  /* We just stole these, the reference must be released */
  rd_kafka_queue_destroy(self->consumer_kafka_queue);
  self->consumer_kafka_queue = NULL;
  rd_kafka_queue_destroy(self->main_kafka_queue);
  self->main_kafka_queue = NULL;

  /* Wait for outstanding requests to finish. */
  kafka_final_flush(self);

  main_loop_worker_run_gc();
}

/* ***********************************************************
 * Strategy - batch consume and direct processing or queueing
 * ***********************************************************/

/* runs in a dedicated thread */
static void
_consumer_run_batch_consume(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  rd_kafka_message_t **msgs = g_new0(rd_kafka_message_t *, self->options.fetch_limit);
  const guint topics_num = g_list_length(self->requested_topics);
  g_assert(topics_num == 1 && g_list_length(self->topic_handle_list) == 1);
  KafkaTopicParts *fist_item = (KafkaTopicParts *)g_list_first(self->requested_topics)->data;
  int32_t requested_partition = (int32_t)GPOINTER_TO_INT(g_list_first(fist_item->partitions)->data);
  rd_kafka_topic_t *single_topic = (rd_kafka_topic_t *)g_list_first(self->topic_handle_list)->data;
  g_assert(self->used_queue_num <= 2 && self->super.num_workers <= 2);
  const guint msg_queue_ndx = 1;
  GAsyncQueue *msg_queue = (self->used_queue_num ? self->msg_queues[msg_queue_ndx] : NULL);

  msg_debug("kafka: consumer poll run started - NOT queued",
            evt_tag_str("group_id", self->group_id),
            evt_tag_str("topic", rd_kafka_topic_name(single_topic)),
            evt_tag_int("partition", requested_partition),
            evt_tag_str("driver", self->super.super.super.id));

  while (FALSE == main_loop_worker_job_quit())
    {
      int qlen = (int)rd_kafka_outq_len(self->kafka);

      rd_kafka_poll(self->kafka, 0);
      ssize_t cnt = rd_kafka_consume_batch_queue(self->consumer_kafka_queue,
                                                 self->options.super.poll_timeout,
                                                 msgs,
                                                 self->options.fetch_limit);
      g_assert(cnt <= self->options.fetch_limit);
      if (cnt < 0 || (cnt == 1 && msgs[0]->err))
        {
          msg_error("kafka: consume batch error",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("topic", rd_kafka_topic_name(single_topic)),
                    evt_tag_int("partition", requested_partition),
                    evt_tag_str("error", cnt < 0 ? rd_kafka_err2str(rd_kafka_last_error()) : rd_kafka_message_errstr(msgs[0])),
                    evt_tag_str("driver", self->super.super.super.id));
          if (cnt == 1)
            rd_kafka_message_destroy(msgs[0]);
          break;
        }

      for (ssize_t i = 0; i < cnt; i++)
        {
          rd_kafka_message_t *msg = msgs[i];
          if (FALSE == main_loop_worker_job_quit())
            {
              if (msg_queue)
                {
                  gint msg_queue_len;
                  while ((msg_queue_len = g_async_queue_length(msg_queue)) >= self->options.fetch_limit)
                    {
                      msg_verbose("kafka: message queue full, waiting",
                                  evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                                  evt_tag_int("msg_queue_len", msg_queue_len));
                      kafka_sd_signal_queue_ndx(self, msg_queue_ndx);
                      rd_kafka_poll(self->kafka, self->options.fetch_queue_full_delay);
                      if (main_loop_worker_job_quit())
                        break;
                    }

                  if (FALSE == main_loop_worker_job_quit())
                    {
                      g_async_queue_push(msg_queue, msg);
                      kafka_sd_signal_queue_ndx(self, msg_queue_ndx);
                      continue;
                    }
                }
              else
                {
                  _process_message(worker, msg);
                  rd_kafka_poll(self->kafka, 0);
                  main_loop_worker_wait_for_exit_until(iteration_sleep_time);
                  continue;
                }
            }
          rd_kafka_message_destroy(msg);
        }

      if (cnt == 0)
        {
          msg_debug("kafka: consume_batch - no data", evt_tag_int("kafka_outq_len", qlen));
          kafka_update_state(self, TRUE);
        }
    }
  rd_kafka_consume_stop(single_topic, requested_partition);
  rd_kafka_queue_destroy(self->consumer_kafka_queue);
  self->consumer_kafka_queue = NULL;

  /* Wait for outstanding requests to finish. */
  kafka_final_flush(self);

  g_free(msgs);
  main_loop_worker_run_gc();
}

static gboolean
_restart_consumer(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  main_loop_worker_wait_for_exit_until(self->options.time_reopen);

  while (FALSE == main_loop_worker_job_quit())
    {
      kafka_update_state(self, TRUE);
      if (g_atomic_counter_get(&self->sleeping_thread_num) == self->super.num_workers - 1)
        {
          kafka_sd_reopen(&self->super.super.super);
          break;
        }
      msg_trace("kafka: waiting for queue processors to sleep",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("driver", self->super.super.super.id));
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
      kafka_update_state(self, TRUE);

      switch(self->startegy)
        {
        case KSCS_ASSIGN_POLL_QUEUED:
        case KSCS_SUBSCRIBE_POLL_QUEUED:
          _consumer_run_consumer_poll_and_queue(worker, iteration_sleep_time);
          break;

        case KSCS_BATCH_CONSUME_DIRECTLY:
          _consumer_run_batch_consume_directly(worker, iteration_sleep_time);
          break;
        default:
          g_assert_not_reached();
        }
      msg_debug("kafka: stopped consumer poll run",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("driver", self->super.super.super.id));

      exit_requested = main_loop_worker_job_quit();

      /* this means an error happened, let's retry from ground */
      if (FALSE == exit_requested)
        exit_requested = (FALSE == _restart_consumer(worker, iteration_sleep_time));
    }
  while (FALSE == exit_requested);

  /* Wake-up sleeping/waiting (queue) processor workers */
  kafka_sd_signal_queues(self);
  /* Wait for all the (queue) processor workers to exit, see _processor_run why not just for the _consumer_run_consumer_poll case */
  while (g_atomic_counter_get(&self->running_thread_num) > 1)
    {
      main_loop_worker_wait_for_exit_until(iteration_sleep_time);
      kafka_sd_signal_queues(self);
    }

  if (self->used_queue_num > 0)
    kafka_sd_drop_queued_messages(self);

  gint running_thread_num = g_atomic_counter_dec_and_test(&self->running_thread_num);
  g_assert(running_thread_num == 1);
}

static void
_exit_requested(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  /* We need to wake-up the possibly blocking rd_kafka_consumer_poll/rd_kafka_poll */
  if (self->consumer_kafka_queue)
    rd_kafka_queue_yield(self->consumer_kafka_queue);
  if (self->main_kafka_queue)
    rd_kafka_queue_yield(self->main_kafka_queue);
}

LogThreadedSourceWorker *kafka_src_worker_new(LogThreadedSourceDriver *owner, gint worker_index)
{
  LogThreadedSourceWorker *self = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(self, owner, worker_index);
  if (worker_index == 0)
    {
      self->run = _consumer_run;
      self->request_exit = _exit_requested;
    }
  else
    self->run = _processor_run;
  return self;
}
