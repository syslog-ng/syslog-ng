/*
 * Copyright (c) 2013, 2014 Balabit
 * Copyright (c) 2013, 2014 Gergely Nagy <algernon@balabit.hu>
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

#include "stats/stats-cluster-logpipe.h"
#include "logthrdestdrv.h"
#include "seqnum.h"
#include "scratch-buffers.h"

#define MAX_RETRIES_OF_FAILED_INSERT_DEFAULT 3

void
log_threaded_dest_driver_set_max_retries(LogDriver *s, gint max_retries)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  self->retries.max = max_retries;
}

static gchar *
_format_seqnum_persist_name(LogThreadedDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name), "%s.seqnum",
             self->super.super.super.generate_persist_name((const LogPipe *)self));

  return persist_name;
}

static void
_start_watches(LogThreadedDestDriver *self)
{
  iv_task_register(&self->do_work);
}

static void
_stop_watches(LogThreadedDestDriver *self)
{
  if (iv_task_registered(&self->do_work))
    {
      iv_task_unregister(&self->do_work);
    }
  if (iv_timer_registered(&self->timer_reopen))
    {
      iv_timer_unregister(&self->timer_reopen);
    }
  if (iv_timer_registered(&self->timer_throttle))
    {
      iv_timer_unregister(&self->timer_throttle);
    }
}

/* NOTE: runs in the worker thread in response to a wakeup event being
 * posted, which happens if a new element is added to our queue while we
 * were sleeping */
static void
_wakeup_event_callback(gpointer data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)data;

  if (!iv_task_registered(&self->do_work))
    {
      iv_task_register(&self->do_work);
    }
}

/* NOTE: runs in the worker thread in response to the shutdown event being
 * posted.  The shutdown event is initiated by the mainloop when the
 * configuration is deinited */
static void
_shutdown_event_callback(gpointer data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)data;

  log_queue_reset_parallel_push(self->worker.queue);
  _stop_watches(self);
  iv_quit();
}

/* NOTE: runs in the worker thread */
static void
_suspend(LogThreadedDestDriver *self)
{
  iv_validate_now();
  self->timer_reopen.expires  = iv_now;
  self->timer_reopen.expires.tv_sec += self->time_reopen;
  iv_timer_register(&self->timer_reopen);
}

/* NOTE: runs in the worker thread */
static void
_connect(LogThreadedDestDriver *self)
{
  self->worker.connected = TRUE;
  if (self->worker.connect)
    {
      self->worker.connected = self->worker.connect(self);
    }

  if (!self->worker.connected)
    {
      _suspend(self);
    }
  else
    {
      _start_watches(self);
    }
}

/* NOTE: runs in the worker thread */
static void
_disconnect(LogThreadedDestDriver *self)
{
  if (self->worker.disconnect)
    {
      self->worker.disconnect(self);
    }
  self->worker.connected = FALSE;
}

/* NOTE: runs in the worker thread */
static void
_disconnect_and_suspend(LogThreadedDestDriver *self)
{
  self->suspended = TRUE;
  _disconnect(self);
  _suspend(self);
}

/* Accepts the current batch including the current message by acking it back
 * to the source.
 *
 * NOTE: runs in the worker thread */
void
_accept_batch(LogThreadedDestDriver *self)
{
  self->retries.counter = 0;
  step_sequence_number(&self->seq_num);
  log_queue_ack_backlog(self->worker.queue, self->batch_size);
  self->batch_size = 0;
}

/* Put the message on the backlog by increasing batch_size.  It will be
 * acknowledged by the next _accept_message(), rewound by the next
 * _rewind_message() or dropped by the next _drop_message() */
void
_queue_message_into_batch(LogThreadedDestDriver *self)
{
  step_sequence_number(&self->seq_num);
}

/* NOTE: runs in the worker thread */
void
_drop_batch(LogThreadedDestDriver *self)
{
  stats_counter_add(self->dropped_messages, self->batch_size);
  _accept_batch(self);
}

/* NOTE: runs in the worker thread */
void
_rewind_batch(LogThreadedDestDriver *self)
{
  log_queue_rewind_backlog(self->worker.queue, self->batch_size);
  self->batch_size = 0;
}

static void
_process_result(LogThreadedDestDriver *self, gint result, LogMessage *triggering_message)
{
  switch (result)
    {
    case WORKER_INSERT_RESULT_DROP:
      msg_error("Message(s) dropped while sending message to destination",
                evt_tag_str("driver", self->super.super.id),
                evt_tag_int("batch_size", self->batch_size));

      _drop_batch(self);
      _disconnect_and_suspend(self);
      break;

    case WORKER_INSERT_RESULT_ERROR:
      self->retries.counter++;

      if (self->retries.counter >= self->retries.max)
        {
          if (self->messages.retry_over && triggering_message)
            self->messages.retry_over(self, triggering_message);

          msg_error("Multiple failures while sending message(s) to destination, message(s) dropped",
                    evt_tag_str("driver", self->super.super.id),
                    evt_tag_int("number_of_retries", self->retries.max),
                    evt_tag_int("batch_size", self->batch_size));

          _drop_batch(self);
        }
      else
        {
          msg_error("Error occurred while trying to send a message, trying again",
                    evt_tag_str("driver", self->super.super.id),
                    evt_tag_int("retries", self->retries.counter),
                    evt_tag_int("batch_size", self->batch_size));
          _rewind_batch(self);
          _disconnect_and_suspend(self);
        }
      break;

    case WORKER_INSERT_RESULT_NOT_CONNECTED:
      msg_debug("Server disconnected while preparing messages for sending, trying again",
                evt_tag_str("driver", self->super.super.id),
                evt_tag_int("batch_size", self->batch_size));
      _rewind_batch(self);
      _disconnect_and_suspend(self);
      break;

    case WORKER_INSERT_RESULT_SUCCESS:
      stats_counter_inc(self->written_messages);
      _accept_batch(self);
      break;

    case WORKER_INSERT_RESULT_QUEUED:
      _queue_message_into_batch(self);
      break;

    default:
      break;
    }

}

/* NOTE: runs in the worker thread, whenever items on our queue are
 * available. It iterates all elements on the queue, however will terminate
 * if the mainloop requests that we exit. */
static void
_perform_inserts(LogThreadedDestDriver *self)
{
  LogMessage *msg;
  worker_insert_result_t result;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  while (G_LIKELY(!self->under_termination) &&
         !self->suspended &&
         (msg = log_queue_pop_head(self->worker.queue, &path_options)) != NULL)
    {
      msg_set_context(msg);
      log_msg_refcache_start_consumer(msg, &path_options);

      ScratchBuffersMarker mark;
      scratch_buffers_mark(&mark);
      result = self->worker.insert(self, msg);
      scratch_buffers_reclaim_marked(mark);

      self->batch_size++;
      _process_result(self, result, msg);

      log_msg_unref(msg);
      msg_set_context(NULL);
      log_msg_refcache_stop();
    }
  if (!self->suspended)
    {
      if (self->worker.worker_message_queue_empty)
        {
          result = self->worker.worker_message_queue_empty(self);
          _process_result(self, result, NULL);
        }
    }
}

/* this callback is invoked by LogQueue and is registered using
 * log_queue_check_items().  This only gets registered if at that point
 * we've decided to wait for the queue, e.g.  the work_task is not running.
 *
 * This callback is invoked from the source thread, e.g.  it is not safe to
 * do anything, but ensure that our thread is woken up in response.
 */
static void
_message_became_available_callback(gpointer user_data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *) user_data;

  if (!self->under_termination)
    iv_event_post(&self->wake_up_event);
}

static void
_perform_work(gpointer data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)data;
  gint timeout_msec = 0;

  self->suspended = FALSE;
  main_loop_worker_run_gc();
  _stop_watches(self);

  if (!self->worker.connected)
    {
      _connect(self);
    }

  else if (log_queue_check_items(self->worker.queue, &timeout_msec,
                                 _message_became_available_callback,
                                 self, NULL))
    {
      _perform_inserts(self);
      if (!self->suspended)
        _start_watches(self);
    }
  else if (timeout_msec != 0)
    {
      /* NOTE: in this case we don't need to reset parallel push
       * notification, as check_items returned with a timeout.  No callback
       * is set up at this time.  */

      iv_validate_now();
      self->timer_throttle.expires = iv_now;
      timespec_add_msec(&self->timer_throttle.expires, timeout_msec);
      iv_timer_register(&self->timer_throttle);
    }
  else
    {
      /* NOTE: at this point we are not doing anything but keep the
       * parallel_push callback alive, which will call
       * _message_became_available_callback(), which in turn wakes us up by
       * posting an event which causes this function to be run again
       *
       * NOTE/2: the parallel_push callback may need to be cancelled if we
       * need to exit.  That happens in the shutdown_event_callback(), or
       * here in this very function, as log_queue_check_items() will cancel
       * outstanding parallel push callbacks automatically.
       */
    }
}

/* these are events of the _worker_ thread and are not registered to the
 * actual main thread.  We basically run our workload in the handler of the
 * do_work task, which might be invoked in a number of ways.
 *
 * Basic states:
 *   1) disconnected state: _perform_work() will try to connect periodically
 *      using the suspend() mechanism, which uses a timer to get up periodically.
 *
 *   2) once connected:
 *      - if messages are already on the queue: flush them
 *
 *      - if no messages are on the queue: schedule
 *        _message_became_available_callback() to be called by the LogQueue.
 *
 *      - if there's an error, disconnect go back to the #1 state above.
 *
 */
static void
_init_watches(LogThreadedDestDriver *self)
{
  IV_EVENT_INIT(&self->wake_up_event);
  self->wake_up_event.cookie = self;
  self->wake_up_event.handler = _wakeup_event_callback;
  iv_event_register(&self->wake_up_event);

  IV_EVENT_INIT(&self->shutdown_event);
  self->shutdown_event.cookie = self;
  self->shutdown_event.handler = _shutdown_event_callback;
  iv_event_register(&self->shutdown_event);

  IV_TIMER_INIT(&self->timer_reopen);
  self->timer_reopen.cookie = self;
  self->timer_reopen.handler = _perform_work;

  IV_TIMER_INIT(&self->timer_throttle);
  self->timer_throttle.cookie = self;
  self->timer_throttle.handler = _perform_work;

  IV_TASK_INIT(&self->do_work);
  self->do_work.cookie = self;
  self->do_work.handler = _perform_work;
}

static void
_worker_thread(gpointer arg)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)arg;

  iv_init();

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.id));

  log_queue_set_use_backlog(self->worker.queue, TRUE);

  _init_watches(self);

  if (self->worker.thread_init)
    self->worker.thread_init(self);

  _start_watches(self);
  iv_main();

  _disconnect(self);

  if (self->worker.thread_deinit)
    self->worker.thread_deinit(self);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->super.super.id));
  iv_deinit();
}

static void
_request_worker_exit(gpointer s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *) s;

  self->under_termination = TRUE;
  iv_event_post(&self->shutdown_event);
}

static void
_start_worker_thread(LogThreadedDestDriver *self)
{
  main_loop_create_worker_thread(_worker_thread,
                                 _request_worker_exit,
                                 self, &self->worker_options);
}

/* the feeding side of the driver, runs in the source thread and puts an
 * incoming message to the associated queue.
 */
static void
log_threaded_dest_driver_queue(LogPipe *s, LogMessage *msg,
                               const LogPathOptions *path_options)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  log_msg_add_ack(msg, path_options);
  log_queue_push_tail(self->worker.queue, log_msg_ref(msg), path_options);

  stats_counter_inc(self->processed_messages);

  log_dest_driver_queue_method(s, msg, path_options);
}

static void
_update_memory_usage_counter_when_fifo_is_used(LogThreadedDestDriver *self)
{
  if (!g_strcmp0(self->worker.queue->type, "FIFO") && self->memory_usage)
    {
      LogPipe *_pipe = &self->super.super.super;
      load_counter_from_persistent_storage(log_pipe_get_config(_pipe), self->memory_usage);
    }
}

static void
_register_stats(LogThreadedDestDriver *self)
{
  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key,self->stats_source | SCS_DESTINATION,
                                self->super.super.id,
                                self->format.stats_instance(self));
  stats_register_counter(0, &sc_key, SC_TYPE_QUEUED, &self->queued_messages);
  stats_register_counter(0, &sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_register_counter_and_index(1, &sc_key, SC_TYPE_MEMORY_USAGE, &self->memory_usage);
  stats_register_counter(0, &sc_key, SC_TYPE_WRITTEN, &self->written_messages);
  stats_unlock();
}

static void
_unregister_stats(LogThreadedDestDriver *self)
{
  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, self->stats_source | SCS_DESTINATION,
                                self->super.super.id,
                                self->format.stats_instance(self));
  stats_unregister_counter(&sc_key, SC_TYPE_QUEUED, &self->queued_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_WRITTEN, &self->written_messages);
  stats_unregister_counter(&sc_key, SC_TYPE_MEMORY_USAGE, &self->memory_usage);
  stats_unlock();
}

gboolean
log_threaded_dest_driver_init_method(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg && self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

  self->worker.queue = log_dest_driver_acquire_queue(
                         &self->super,
                         s->generate_persist_name(s));

  if (self->worker.queue == NULL)
    return FALSE;

  _register_stats(self);

  log_queue_set_counters(self->worker.queue, self->queued_messages,
                         self->dropped_messages, self->memory_usage);
  _update_memory_usage_counter_when_fifo_is_used(self);

  self->seq_num = GPOINTER_TO_INT(cfg_persist_config_fetch(cfg,
                                                           _format_seqnum_persist_name(self)));
  if (!self->seq_num)
    init_sequence_number(&self->seq_num);

  _start_worker_thread(self);

  return TRUE;
}

gboolean
log_threaded_dest_driver_deinit_method(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;


  log_queue_set_counters(self->worker.queue, NULL, NULL, NULL);

  cfg_persist_config_add(log_pipe_get_config(s),
                         _format_seqnum_persist_name(self),
                         GINT_TO_POINTER(self->seq_num), NULL, FALSE);

  save_counter_to_persistent_storage(log_pipe_get_config(s), self->memory_usage);

  _unregister_stats(self);

  return log_dest_driver_deinit_method(s);
}


void
log_threaded_dest_driver_free(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  log_dest_driver_free((LogPipe *)self);
}

void
log_threaded_dest_driver_init_instance(LogThreadedDestDriver *self, GlobalConfig *cfg)
{
  log_dest_driver_init_instance(&self->super, cfg);

  self->worker_options.is_output_thread = TRUE;

  self->super.super.super.init = log_threaded_dest_driver_init_method;
  self->super.super.super.deinit = log_threaded_dest_driver_deinit_method;
  self->super.super.super.queue = log_threaded_dest_driver_queue;
  self->super.super.super.free_fn = log_threaded_dest_driver_free;
  self->time_reopen = -1;
  self->batch_size = 0;

  self->retries.max = MAX_RETRIES_OF_FAILED_INSERT_DEFAULT;
}
