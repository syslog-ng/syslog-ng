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


/* LogThreadedDestWorker */


void
log_threaded_dest_driver_set_flush_lines(LogDriver *s, gint flush_lines)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *) s;

  self->flush_lines = flush_lines;
}

void
log_threaded_dest_driver_set_flush_timeout(LogDriver *s, gint flush_timeout)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *) s;

  self->flush_timeout = flush_timeout;
}


/* this should be used in combination with WORKER_INSERT_RESULT_EXPLICIT_ACK_MGMT to actually confirm message delivery. */
void
log_threaded_dest_driver_ack_messages(LogThreadedDestDriver *self, gint batch_size)
{
  log_queue_ack_backlog(self->worker.instance.queue, batch_size);
  stats_counter_add(self->written_messages, batch_size);
  self->retries_counter = 0;
  self->batch_size -= batch_size;
}

void
log_threaded_dest_driver_drop_messages(LogThreadedDestDriver *self, gint batch_size)
{
  log_queue_ack_backlog(self->worker.instance.queue, batch_size);
  stats_counter_add(self->dropped_messages, batch_size);
  self->retries_counter = 0;
  self->batch_size -= batch_size;
}

void
log_threaded_dest_driver_rewind_messages(LogThreadedDestDriver *self, gint batch_size)
{
  log_queue_rewind_backlog(self->worker.instance.queue, batch_size);
  self->rewound_batch_size = self->batch_size;
  self->batch_size -= batch_size;
}

static gboolean
_should_flush_now(LogThreadedDestDriver *self)
{
  struct timespec now;
  glong diff;

  if (self->flush_timeout <= 0 ||
      self->flush_lines <= 1 ||
      !self->enable_flush_timeout)
    return TRUE;

  iv_validate_now();
  now = iv_now;
  diff = timespec_diff_msec(&now, &self->last_flush_time);

  return (diff >= self->flush_timeout);
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
  if (iv_timer_registered(&self->timer_flush))
    {
      iv_timer_unregister(&self->timer_flush);
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

  log_queue_reset_parallel_push(self->worker.instance.queue);
  _stop_watches(self);
  iv_quit();
}

/* NOTE: runs in the worker thread */
static void
_suspend(LogThreadedDestDriver *self)
{
  self->suspended = TRUE;
}

/* NOTE: runs in the worker thread */
static void
_connect(LogThreadedDestDriver *self)
{
  if (!log_threaded_dest_worker_connect(self))
    {
      msg_debug("Error establishing connection to server",
                evt_tag_str("driver", self->super.super.id),
                log_expr_node_location_tag(self->super.super.super.expr_node));
      _suspend(self);
    }
}

/* NOTE: runs in the worker thread */
static void
_disconnect(LogThreadedDestDriver *self)
{
  log_threaded_dest_worker_disconnect(self);
}

/* NOTE: runs in the worker thread */
static void
_disconnect_and_suspend(LogThreadedDestDriver *self)
{
  _disconnect(self);
  _suspend(self);
}

/* Accepts the current batch including the current message by acking it back
 * to the source.
 *
 * NOTE: runs in the worker thread */
static void
_accept_batch(LogThreadedDestDriver *self)
{
  log_threaded_dest_driver_ack_messages(self, self->batch_size);
}

/* NOTE: runs in the worker thread */
static void
_drop_batch(LogThreadedDestDriver *self)
{
  log_threaded_dest_driver_drop_messages(self, self->batch_size);
}

/* NOTE: runs in the worker thread */
static void
_rewind_batch(LogThreadedDestDriver *self)
{
  log_threaded_dest_driver_rewind_messages(self, self->batch_size);
}

static void
_process_result(LogThreadedDestDriver *self, gint result)
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
      self->retries_counter++;

      if (self->retries_counter >= self->retries_max)
        {
          msg_error("Multiple failures while sending message(s) to destination, message(s) dropped",
                    evt_tag_str("driver", self->super.super.id),
                    log_expr_node_location_tag(self->super.super.super.expr_node),
                    evt_tag_int("retries", self->retries_counter),
                    evt_tag_int("batch_size", self->batch_size));

          _drop_batch(self);
        }
      else
        {
          msg_error("Error occurred while trying to send a message, trying again",
                    evt_tag_str("driver", self->super.super.id),
                    log_expr_node_location_tag(self->super.super.super.expr_node),
                    evt_tag_int("retries", self->retries_counter),
                    evt_tag_int("batch_size", self->batch_size));
          _rewind_batch(self);
          _disconnect_and_suspend(self);
        }
      break;

    case WORKER_INSERT_RESULT_NOT_CONNECTED:
      msg_info("Server disconnected while preparing messages for sending, trying again",
               evt_tag_str("driver", self->super.super.id),
               log_expr_node_location_tag(self->super.super.super.expr_node),
               evt_tag_int("batch_size", self->batch_size));
      _rewind_batch(self);
      _disconnect_and_suspend(self);
      break;

    case WORKER_INSERT_RESULT_EXPLICIT_ACK_MGMT:
      /* we require the instance to use explicit calls to ack_messages/rewind_messages */
      break;

    case WORKER_INSERT_RESULT_SUCCESS:
      _accept_batch(self);
      break;

    case WORKER_INSERT_RESULT_QUEUED:
      self->enable_flush_timeout = TRUE;
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

  if (self->batch_size == 0)
    {
      /* first message in the batch sets the last_flush_time, so we
       * won't expedite the flush even if the previous one was a long
       * time ago */

      iv_validate_now();
      self->last_flush_time = iv_now;
    }

  while (G_LIKELY(!self->under_termination) &&
         !self->suspended &&
         (msg = log_queue_pop_head(self->worker.instance.queue, &path_options)) != NULL)
    {
      msg_set_context(msg);
      log_msg_refcache_start_consumer(msg, &path_options);

      self->batch_size++;
      ScratchBuffersMarker mark;
      scratch_buffers_mark(&mark);

      result = log_threaded_dest_worker_insert(self, msg);
      scratch_buffers_reclaim_marked(mark);

      step_sequence_number(&self->seq_num);

      _process_result(self, result);

      log_msg_unref(msg);
      msg_set_context(NULL);
      log_msg_refcache_stop();

      if (self->rewound_batch_size)
        {
          self->rewound_batch_size--;
          if (self->rewound_batch_size == 0)
            break;
        }

      iv_invalidate_now();
    }
  self->rewound_batch_size = 0;
}

static void
_perform_flush(LogThreadedDestDriver *self)
{
  /* NOTE: earlier we had a condition on only calling flush() if batch_size
   * is non-zero.  This was removed, as the language bindings that were done
   * _before_ the batching support in LogThreadedDestDriver relies on
   * flush() being called always, even if WORKER_INSERT_RESULT_SUCCESS is
   * returned, in which case batch_size is already zero at this point.
   */
  if (!self->suspended)
    {
      msg_trace("flushing batch",
                evt_tag_str("driver", self->super.super.id),
                evt_tag_int("batch_size", self->batch_size));

      worker_insert_result_t result = log_threaded_dest_worker_flush(self);
      _process_result(self, result);
    }

  iv_invalidate_now();
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
_schedule_restart_on_suspend_timeout(LogThreadedDestDriver *self)
{
  iv_validate_now();
  self->timer_reopen.expires  = iv_now;
  self->timer_reopen.expires.tv_sec += self->time_reopen;
  iv_timer_register(&self->timer_reopen);
}

static void
_schedule_restart_on_flush_timeout(LogThreadedDestDriver *self)
{
  self->timer_flush.expires = self->last_flush_time;
  timespec_add_msec(&self->timer_flush.expires, self->flush_timeout);
  iv_timer_register(&self->timer_flush);
}

static void
_schedule_restart(LogThreadedDestDriver *self)
{
  if (self->suspended)
    _schedule_restart_on_suspend_timeout(self);
  else
    iv_task_register(&self->do_work);
}

static void
_schedule_restart_on_next_flush(LogThreadedDestDriver *self)
{
  if (self->suspended)
    _schedule_restart_on_suspend_timeout(self);
  else if (!_should_flush_now(self))
    _schedule_restart_on_flush_timeout(self);
  else
    iv_task_register(&self->do_work);
}

static void
_schedule_restart_on_throttle_timeout(LogThreadedDestDriver *self, gint timeout_msec)
{
  iv_validate_now();
  self->timer_throttle.expires = iv_now;
  timespec_add_msec(&self->timer_throttle.expires, timeout_msec);
  iv_timer_register(&self->timer_throttle);
}

static void
_perform_work(gpointer data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)data;
  gint timeout_msec = 0;

  self->suspended = FALSE;
  main_loop_worker_run_gc();
  _stop_watches(self);

  if (!self->worker.instance.connected)
    {
      /* try to connect and come back if successful, would be suspended otherwise. */
      _connect(self);
      _schedule_restart(self);
    }
  else if (log_queue_check_items(self->worker.instance.queue, &timeout_msec,
                                 _message_became_available_callback,
                                 self, NULL))
    {

      msg_trace("message(s) available in queue",
                evt_tag_str("driver", self->super.super.id));

      /* Something is in the queue, buffer them up and flush (if needed) */
      _perform_inserts(self);
      if (_should_flush_now(self))
        _perform_flush(self);
      _schedule_restart(self);
    }
  else if (self->batch_size > 0)
    {
      /* nothing in the queue, but there are pending elements in the buffer
       * (e.g.  batch size != 0).  perform a round of flushing.  We might
       * get back here, as the flush() routine doesn't have to flush
       * everything.  We are awoken either by the
       * _message_became_available_callback() or if the next flush time has
       * arrived.  */
      msg_trace("queue empty",
                evt_tag_str("driver", self->super.super.id));

      if (_should_flush_now(self))
        _perform_flush(self);
      _schedule_restart_on_next_flush(self);
    }
  else if (timeout_msec != 0)
    {
      /* We probably have some items in the queue, but timeout_msec is set,
       * indicating a throttle being active.
       * _message_became_available_callback() is not set up in this case.
       * we need to wake up after timeout_msec time.
       *
       * We are processing throttle delays _after_ we finished flushing, as
       * items in the queue were already accepted by throttling, so they can
       * be flushed.
       */
      msg_trace("delay due to throttling",
                evt_tag_int("timeout_msec", timeout_msec),
                evt_tag_str("driver", self->super.super.id));

      _schedule_restart_on_throttle_timeout(self, timeout_msec);

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

static void
_flush_timer_cb(gpointer data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)data;
  msg_debug("flush timer expired",
            evt_tag_str("driver", self->super.super.id),
            evt_tag_int("batch_size", self->batch_size));
  _perform_work(data);
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

  IV_EVENT_INIT(&self->shutdown_event);
  self->shutdown_event.cookie = self;
  self->shutdown_event.handler = _shutdown_event_callback;

  IV_TIMER_INIT(&self->timer_reopen);
  self->timer_reopen.cookie = self;
  self->timer_reopen.handler = _perform_work;

  IV_TIMER_INIT(&self->timer_throttle);
  self->timer_throttle.cookie = self;
  self->timer_throttle.handler = _perform_work;

  IV_TIMER_INIT(&self->timer_flush);
  self->timer_flush.cookie = self;
  self->timer_flush.handler = _flush_timer_cb;

  IV_TASK_INIT(&self->do_work);
  self->do_work.cookie = self;
  self->do_work.handler = _perform_work;
}

static void
_signal_startup_finished(LogThreadedDestDriver *self)
{
  g_mutex_lock(self->lock);
  self->startup_finished = TRUE;
  g_cond_signal(self->started_up);
  g_mutex_unlock(self->lock);
}

static void
_wait_for_startup_finished(LogThreadedDestDriver *self)
{
  g_mutex_lock(self->lock);
  while (!self->startup_finished)
    g_cond_wait(self->started_up, self->lock);
  g_mutex_unlock(self->lock);
}

static void
_worker_thread(gpointer arg)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)arg;

  iv_init();

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.id));

  log_queue_set_use_backlog(self->worker.instance.queue, TRUE);

  iv_event_register(&self->wake_up_event);
  iv_event_register(&self->shutdown_event);

  log_threaded_dest_worker_thread_init(self);

  _signal_startup_finished(self);

  /* if we have anything on the backlog, that was a partial, potentially
   * not-flushed batch.  Rewind it, so we start with that */

  log_queue_rewind_backlog_all(self->worker.instance.queue);

  _schedule_restart(self);
  iv_main();

  log_threaded_dest_worker_flush(self);
  log_queue_rewind_backlog_all(self->worker.instance.queue);

  _disconnect(self);

  log_threaded_dest_worker_thread_deinit(self);

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
  _wait_for_startup_finished(self);
}

/* LogThreadedDestDriver */

void
log_threaded_dest_driver_set_max_retries(LogDriver *s, gint max_retries)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  self->retries_max = max_retries;
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
  log_queue_push_tail(self->worker.instance.queue, log_msg_ref(msg), path_options);

  stats_counter_inc(self->processed_messages);

  log_dest_driver_queue_method(s, msg, path_options);
}

static void
_register_stats(LogThreadedDestDriver *self)
{
  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key,self->stats_source | SCS_DESTINATION,
                                  self->super.super.id,
                                  self->format_stats_instance(self));
    stats_register_counter(0, &sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
    stats_register_counter(0, &sc_key, SC_TYPE_WRITTEN, &self->written_messages);
    log_queue_register_stats_counters(self->worker.instance.queue, 0, &sc_key);
  }
  stats_unlock();
}

static void
_unregister_stats(LogThreadedDestDriver *self)
{
  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, self->stats_source | SCS_DESTINATION,
                                  self->super.super.id,
                                  self->format_stats_instance(self));
    stats_unregister_counter(&sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_WRITTEN, &self->written_messages);
    log_queue_unregister_stats_counters(self->worker.instance.queue, &sc_key);
  }
  stats_unlock();
}

gboolean
log_threaded_dest_driver_init_method(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg && self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

  if (cfg && self->flush_lines == -1)
    self->flush_lines = cfg->flush_lines;

  if (cfg && self->flush_timeout == -1)
    self->flush_timeout = cfg->flush_timeout;

  self->worker.instance.queue = log_dest_driver_acquire_queue(
                         &self->super,
                         s->generate_persist_name(s));

  if (self->worker.instance.queue == NULL)
    return FALSE;

  _register_stats(self);

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

  cfg_persist_config_add(log_pipe_get_config(s),
                         _format_seqnum_persist_name(self),
                         GINT_TO_POINTER(self->seq_num), NULL, FALSE);

  _unregister_stats(self);

  return log_dest_driver_deinit_method(s);
}


void
log_threaded_dest_driver_free(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  g_mutex_free(self->lock);
  g_cond_free(self->started_up);
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
  self->flush_lines = -1;
  self->flush_timeout = -1;

  self->retries_max = MAX_RETRIES_OF_FAILED_INSERT_DEFAULT;
  _init_watches(self);
  self->lock = g_mutex_new();
  self->started_up = g_cond_new();
}
