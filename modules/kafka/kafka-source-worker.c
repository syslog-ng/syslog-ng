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
_mainloop_sleep_time(const gdouble delay)
{
  return delay > 0 ? 1.0 / delay : 0.0;
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
  kafka_sd_update_msg_length_stats(self, msg_len);
  log_msg_set_recvd_rawmsg_size(log_msg, msg->len);
  kafka_sd_inc_msg_topic_stats(self, rd_kafka_topic_name(msg->rkt));

  _send(worker, log_msg);

  rd_kafka_error_t *err = rd_kafka_offset_store_message(msg);
  if (err)
    msg_warning("kafka: error storing message offset",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
                evt_tag_int("partition", msg->partition),
                evt_tag_str("error", rd_kafka_error_string(err)),
                evt_tag_str("driver", self->super.super.super.id));

  rd_kafka_message_destroy(msg);
  main_loop_worker_run_gc();

  return TRUE;
}

static inline ThreadedFetchResult
_fetch(LogThreadedSourceWorker *worker, rd_kafka_message_t **msg)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  GAsyncQueue *msg_queue = kafka_sd_worker_queue(self, worker);

  *msg = g_async_queue_try_pop(msg_queue);
  if (G_LIKELY(*msg != NULL))
    {
      LogThreadedSourceWorker *target_worker = (self->options.separated_worker_queues ?
                                                self->super.workers[worker->worker_index] :
                                                self->super.workers[1]); /* Intentionally not using the 0 index slot */
      /* Setting directly is still racy, but we can live with that */
      guint msg_queue_len = g_async_queue_length(msg_queue);
      kafka_sd_set_msg_worker_stats(self, kafka_src_worker_get_name(target_worker), msg_queue_len);
      return THREADED_FETCH_SUCCESS;
    }
  return THREADED_FETCH_NO_DATA;
}

/* runs in a dedicated thread */
static void
_processor_run(LogThreadedSourceWorker *worker)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  g_atomic_counter_inc(&self->running_thread_num);

  kafka_msg_debug("kafka: started queue processor",
                  evt_tag_int("index", worker->worker_index),
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("driver", self->super.super.super.id));

  GAsyncQueue *msg_queue = kafka_sd_worker_queue(self, worker);
  const gdouble iteration_sleep_time = _mainloop_sleep_time(self->options.fetch_delay);
  const gdouble fetch_retry_sleep_time = _mainloop_sleep_time(self->options.fetch_retry_delay);
  rd_kafka_message_t *msg = NULL;

  while (1)
    {
      if (G_UNLIKELY(main_loop_worker_job_quit()))
        break;

      /* Lazy check for empty queue */
      if (g_async_queue_length(msg_queue) == 0)
        kafka_sd_wait_for_queue(self, worker);

      while (1)
        {
          if (G_UNLIKELY(main_loop_worker_job_quit()))
            break;

          if (G_LIKELY(_fetch(worker, &msg) == THREADED_FETCH_SUCCESS))
            {
              _process_message(worker, msg);
              rd_kafka_poll(self->kafka, 0);
              main_loop_worker_wait_for_exit_until(iteration_sleep_time);
              continue;
            }

          main_loop_worker_wait_for_exit_until(fetch_retry_sleep_time);
          break;
        }
    }
  kafka_msg_debug("kafka: stopped queue processor",
                  evt_tag_int("index", worker->worker_index),
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("driver", self->super.super.super.id),
                  evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                  evt_tag_int("worker_queue_len", g_async_queue_length(msg_queue)));
  g_atomic_counter_dec_and_test(&self->running_thread_num);
}

static gboolean
_queue_message(LogThreadedSourceWorker *worker, rd_kafka_message_t *msg, guint target_queue_ndx)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;

  msg_trace("kafka: queuing message",
            evt_tag_str("topic", rd_kafka_topic_name(msg->rkt)),
            evt_tag_int("partition", msg->partition),
            evt_tag_int("offset", msg->offset),
            evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
            evt_tag_int("worker_queues_len", kafka_sd_worker_queues_len(self)));
  do
    {
      gint msg_queues_len = kafka_sd_worker_queues_len(self);

      if (G_LIKELY(msg_queues_len < self->options.fetch_limit))
        break;

      kafka_msg_debug("kafka: message queue full, waiting",
                      evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                      evt_tag_int("worker_queues_len", msg_queues_len));
      kafka_sd_signal_queues(self);
      rd_kafka_poll(self->kafka, self->options.fetch_queue_full_delay);

      if (G_UNLIKELY(main_loop_worker_job_quit()))
        return FALSE;
    }
  while (1);

  g_async_queue_push(self->msg_queues[target_queue_ndx], msg);
  kafka_sd_signal_queue_ndx(self, target_queue_ndx);

  /* Setting queue stats directly is still racy, but we can live with that */
  gint msg_queue_len = g_async_queue_length(self->msg_queues[target_queue_ndx]);
  const gchar *worker_name = kafka_src_worker_get_name(self->super.workers[target_queue_ndx]);
  kafka_sd_set_msg_worker_stats(self, worker_name, msg_queue_len);

  return TRUE;
}

static inline guint
_next_target_queue_ndx(KafkaSourceDriver *self, guint *rr)
{
  guint target_queue_ndx = (self->options.separated_worker_queues ?
                            ((*rr)++ % (self->super.num_workers - 1)) + 1 :
                            1); /* Intentionally not using the 0 index slot */
  return target_queue_ndx;
}

/* *************************************************************************
 * Strategy - assign/subscribe consumer poll and queueing or direct processing
 * *************************************************************************/

/* runs in a dedicated thread */
static void
_consumer_run_consumer_poll(LogThreadedSourceWorker *worker, const gdouble iteration_sleep_time)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) worker->control;
  rd_kafka_message_t *msg;
  guint rr = 0;

  kafka_msg_debug("kafka: consumer poll run started",
                  evt_tag_int("worker_num", self->super.num_workers),
                  evt_tag_int("queue_num", self->used_queue_num ? self->used_queue_num - 1 : 0),
                  evt_tag_int("queues_max_size", self->options.fetch_limit),
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("driver", self->super.super.super.id));
  if (self->strategy == KSCS_SUBSCRIBE)
    kafka_msg_debug("kafka: waiting for group rebalancer",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("driver", self->super.super.super.id));

  /* We just steal these from the main/consumer event loops to be able to stop the blocking rd_kafka_consumer_poll */
  self->consumer_kafka_queue = rd_kafka_queue_get_consumer(self->kafka);
  self->main_kafka_queue = rd_kafka_queue_get_main(self->kafka);

  while (1)
    {
      if (G_UNLIKELY(main_loop_worker_job_quit()))
        break;

      rd_kafka_poll(self->kafka, 0);
      msg = rd_kafka_consumer_poll(self->kafka, self->options.super.poll_timeout);

      if (msg == NULL || msg->err)
        {
          kafka_update_state(self, TRUE);
          if (msg == NULL || msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
            {
              kafka_msg_debug("kafka: consumer_poll - no data",
                              evt_tag_int("kafka_outq_len", (int)rd_kafka_outq_len(self->kafka)),
                              evt_tag_int("worker_queues_len", kafka_sd_worker_queues_len(self)));
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
          if (self->used_queue_num > 0)
            {
              if (G_UNLIKELY(FALSE == _queue_message(worker, msg, _next_target_queue_ndx(self, &rr))))
                rd_kafka_message_destroy(msg);
            }
          else
            {
              _process_message(worker, msg);
              rd_kafka_poll(self->kafka, 0);
              main_loop_worker_wait_for_exit_until(iteration_sleep_time);
            }
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

  /* Wait for outstanding requests to finish, commit offsets */
  kafka_final_flush(self, TRUE);

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
  guint rr = 0;

  kafka_msg_debug("kafka: batch consumer poll run started",
                  evt_tag_int("worker_num", self->super.num_workers),
                  evt_tag_int("queue_num", self->used_queue_num ? self->used_queue_num - 1 : 0),
                  evt_tag_int("queues_max_size", self->options.fetch_limit),
                  evt_tag_str("group_id", self->group_id),
                  evt_tag_str("driver", self->super.super.super.id));

  while (1)
    {
      if (G_UNLIKELY(main_loop_worker_job_quit()))
        break;

      rd_kafka_poll(self->kafka, 0);

      int qlen = (int)rd_kafka_outq_len(self->kafka);
      ssize_t cnt = rd_kafka_consume_batch_queue(self->consumer_kafka_queue,
                                                 self->options.super.poll_timeout,
                                                 msgs,
                                                 self->options.fetch_limit);
      g_assert(cnt <= self->options.fetch_limit);
      if (cnt < 0 || (cnt == 1 && msgs[0]->err))
        {
          msg_error("kafka: consume batch error",
                    evt_tag_str("group_id", self->group_id),
                    evt_tag_str("error", cnt < 0 ? rd_kafka_err2str(rd_kafka_last_error()) : rd_kafka_message_errstr(msgs[0])),
                    evt_tag_str("driver", self->super.super.super.id));
          if (cnt == 1)
            rd_kafka_message_destroy(msgs[0]);
          break;
        }

      for (ssize_t i = 0; i < cnt; i++)
        {
          rd_kafka_message_t *msg = msgs[i];
          if (G_LIKELY(FALSE == main_loop_worker_job_quit()))
            {
              if (self->used_queue_num > 0)
                {
                  if (G_LIKELY(_queue_message(worker, msg, _next_target_queue_ndx(self, &rr))))
                    continue;
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
          kafka_msg_debug("kafka: consume_batch - no data", evt_tag_int("kafka_outq_len", qlen));
          kafka_update_state(self, TRUE);
        }
    }
  /* Stop consuming from all the topics */
  kafka_consume_stop(self->topic_handle_list, self->assigned_partitions);

  /* Wait for outstanding requests to finish, commit offsets */
  kafka_final_flush(self, TRUE);

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
  const gdouble iteration_sleep_time = _mainloop_sleep_time(self->options.fetch_delay);
  gboolean exit_requested = FALSE;

  g_atomic_counter_inc(&self->running_thread_num);
  do
    {
      kafka_update_state(self, TRUE);

      switch(self->strategy)
        {
        case KSCS_ASSIGN:
        case KSCS_SUBSCRIBE:
          _consumer_run_consumer_poll(worker, iteration_sleep_time);
          break;

        case KSCS_BATCH_CONSUME:
          _consumer_run_batch_consume(worker, iteration_sleep_time);
          break;
        default:
          g_assert_not_reached();
        }
      kafka_msg_debug("kafka: stopped consumer poll run",
                      evt_tag_str("group_id", self->group_id),
                      evt_tag_str("driver", self->super.super.super.id));

      exit_requested = main_loop_worker_job_quit();
      if (FALSE == exit_requested)
        {
          /* Getting here means an error happened in the consumer, let's retry from ground */
          exit_requested = (FALSE == _restart_consumer(worker, iteration_sleep_time));
        }
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
  kafka_sd_wakeup_kafka_queues(self);
}

static gboolean
_kafka_src_worker_init(LogThreadedSourceWorker *worker)
{
  KafkaSourceWorker *self = (KafkaSourceWorker *) worker;

  int ret = g_snprintf(self->name, sizeof(self->name), "worker#%d", worker->worker_index);
  g_assert((gsize)ret < sizeof(self->name));

  if (worker->worker_index == 0)
    {
      self->super.run = _consumer_run;
      self->super.request_exit = _exit_requested;
    }
  else
    self->super.run = _processor_run;

  return TRUE;
}

const gchar *
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
   */
  _kafka_src_worker_init(&self->super);

  return &self->super;
}
