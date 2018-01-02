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

#define MAX_RETRIES_OF_FAILED_INSERT_DEFAULT 3

static gchar *
log_threaded_dest_driver_format_seqnum_for_persist(LogThrDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name), "%s.seqnum",
             self->super.super.super.generate_persist_name((const LogPipe *)self));

  return persist_name;
}

static void
log_threaded_dest_driver_suspend(LogThrDestDriver *self)
{
  iv_validate_now();
  self->timer_reopen.expires  = iv_now;
  self->timer_reopen.expires.tv_sec += self->time_reopen;
  iv_timer_register(&self->timer_reopen);
}

static void
log_threaded_dest_driver_message_became_available_in_the_queue(gpointer user_data)
{
  LogThrDestDriver *self = (LogThrDestDriver *) user_data;
  if (!self->under_termination)
    iv_event_post(&self->wake_up_event);
}

static void
log_threaded_dest_driver_wake_up(gpointer data)
{
  LogThrDestDriver *self = (LogThrDestDriver *)data;

  if (!iv_task_registered(&self->do_work))
    {
      iv_task_register(&self->do_work);
    }
}

static void
log_threaded_dest_driver_start_watches(LogThrDestDriver *self)
{
  iv_task_register(&self->do_work);
}

static void
log_threaded_dest_driver_stop_watches(LogThrDestDriver *self)
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

static void
log_threaded_dest_driver_shutdown(gpointer data)
{
  LogThrDestDriver *self = (LogThrDestDriver *)data;
  log_threaded_dest_driver_stop_watches(self);
  iv_quit();
}


static void
__connect(LogThrDestDriver *self)
{
  self->worker.connected = TRUE;
  if (self->worker.connect)
    {
      self->worker.connected = self->worker.connect(self);
    }

  if (!self->worker.connected)
    {
      log_queue_reset_parallel_push(self->queue);
      log_threaded_dest_driver_suspend(self);
    }
  else
    {
      log_threaded_dest_driver_start_watches(self);
    }
}

static void
__disconnect(LogThrDestDriver *self)
{
  if (self->worker.disconnect)
    {
      self->worker.disconnect(self);
    }
  self->worker.connected = FALSE;
}



static void
_disconnect_and_suspend(LogThrDestDriver *self)
{
  self->suspended = TRUE;
  __disconnect(self);
  log_queue_reset_parallel_push(self->queue);
  log_threaded_dest_driver_suspend(self);
}

static void
log_threaded_dest_driver_do_insert(LogThrDestDriver *self)
{
  LogMessage *msg;
  worker_insert_result_t result;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  while (G_LIKELY(!self->under_termination) &&
         !self->suspended &&
         (msg = log_queue_pop_head(self->queue, &path_options)) != NULL)
    {
      msg_set_context(msg);
      log_msg_refcache_start_consumer(msg, &path_options);

      result = self->worker.insert(self, msg);

      switch (result)
        {
        case WORKER_INSERT_RESULT_DROP:
          msg_error("Message dropped while sending message to destination",
                    evt_tag_str("driver", self->super.super.id));

          log_threaded_dest_driver_message_drop(self, msg);
          _disconnect_and_suspend(self);
          break;

        case WORKER_INSERT_RESULT_ERROR:
          self->retries.counter++;

          if (self->retries.counter >= self->retries.max)
            {
              if (self->messages.retry_over)
                self->messages.retry_over(self, msg);

              msg_error("Multiple failures while sending message to destination, message dropped",
                        evt_tag_str("driver", self->super.super.id),
                        evt_tag_int("number_of_retries", self->retries.max));

              log_threaded_dest_driver_message_drop(self, msg);
            }
          else
            {
              log_threaded_dest_driver_message_rewind(self, msg);
              _disconnect_and_suspend(self);
            }
          break;

        case WORKER_INSERT_RESULT_NOT_CONNECTED:
          log_threaded_dest_driver_message_rewind(self, msg);
          _disconnect_and_suspend(self);
          break;

        case WORKER_INSERT_RESULT_REWIND:
          log_threaded_dest_driver_message_rewind(self, msg);
          break;

        case WORKER_INSERT_RESULT_SUCCESS:
          stats_counter_inc(self->written_messages);
          log_threaded_dest_driver_message_accept(self, msg);
          break;

        default:
          break;
        }

      msg_set_context(NULL);
      log_msg_refcache_stop();
    }
  if (!self->suspended)
    {
      if (self->worker.worker_message_queue_empty)
        {
          self->worker.worker_message_queue_empty(self);
        }
    }
}

static void
log_threaded_dest_driver_do_work(gpointer data)
{
  LogThrDestDriver *self = (LogThrDestDriver *)data;
  gint timeout_msec = 0;

  self->suspended = FALSE;
  main_loop_worker_run_gc();
  log_threaded_dest_driver_stop_watches(self);

  if (!self->worker.connected)
    {
      __connect(self);
    }

  else if (log_queue_check_items(self->queue, &timeout_msec,
                                 log_threaded_dest_driver_message_became_available_in_the_queue,
                                 self, NULL))
    {
      log_threaded_dest_driver_do_insert(self);
      if (!self->suspended)
        log_threaded_dest_driver_start_watches(self);
    }
  else if (timeout_msec != 0)
    {
      log_queue_reset_parallel_push(self->queue);
      iv_validate_now();
      self->timer_throttle.expires = iv_now;
      timespec_add_msec(&self->timer_throttle.expires, timeout_msec);
      iv_timer_register(&self->timer_throttle);
    }
}

static void
log_threaded_dest_driver_init_watches(LogThrDestDriver *self)
{
  IV_EVENT_INIT(&self->wake_up_event);
  self->wake_up_event.cookie = self;
  self->wake_up_event.handler = log_threaded_dest_driver_wake_up;
  iv_event_register(&self->wake_up_event);

  IV_EVENT_INIT(&self->shutdown_event);
  self->shutdown_event.cookie = self;
  self->shutdown_event.handler = log_threaded_dest_driver_shutdown;
  iv_event_register(&self->shutdown_event);

  IV_TIMER_INIT(&self->timer_reopen);
  self->timer_reopen.cookie = self;
  self->timer_reopen.handler = log_threaded_dest_driver_do_work;

  IV_TIMER_INIT(&self->timer_throttle);
  self->timer_throttle.cookie = self;
  self->timer_throttle.handler = log_threaded_dest_driver_do_work;

  IV_TASK_INIT(&self->do_work);
  self->do_work.cookie = self;
  self->do_work.handler = log_threaded_dest_driver_do_work;
}

static void
log_threaded_dest_driver_worker_thread_main(gpointer arg)
{
  LogThrDestDriver *self = (LogThrDestDriver *)arg;

  iv_init();

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.id));

  log_queue_set_use_backlog(self->queue, TRUE);

  log_threaded_dest_driver_init_watches(self);

  log_threaded_dest_driver_start_watches(self);

  if (self->worker.thread_init)
    self->worker.thread_init(self);

  iv_main();

  __disconnect(self);
  if (self->worker.thread_deinit)
    self->worker.thread_deinit(self);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->super.super.id));
  iv_deinit();
}

static void
log_threaded_dest_driver_stop_thread(gpointer s)
{
  LogThrDestDriver *self = (LogThrDestDriver *) s;
  self->under_termination = TRUE;
  iv_event_post(&self->shutdown_event);
}

static void
log_threaded_dest_driver_start_thread(LogThrDestDriver *self)
{
  main_loop_create_worker_thread(log_threaded_dest_driver_worker_thread_main,
                                 log_threaded_dest_driver_stop_thread,
                                 self, &self->worker_options);
}

static void
_update_memory_usage_counter_when_fifo_is_used(LogThrDestDriver *self)
{
  if (!g_strcmp0(self->queue->type, "FIFO") && self->memory_usage)
    {
      LogPipe *_pipe = &self->super.super.super;
      load_counter_from_persistent_storage(log_pipe_get_config(_pipe), self->memory_usage);
    }
}

gboolean
log_threaded_dest_driver_start(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg && self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

  self->queue = log_dest_driver_acquire_queue(
                  &self->super, self->super.super.super.generate_persist_name((const LogPipe *)self));

  if (self->queue == NULL)
    {
      return FALSE;
    }

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key,self->stats_source | SCS_DESTINATION,
                                self->super.super.id,
                                self->format.stats_instance(self));
  stats_register_counter(0, &sc_key, SC_TYPE_QUEUED, &self->queued_messages);
  stats_register_counter(0, &sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
  stats_register_counter_and_index(1, &sc_key, SC_TYPE_MEMORY_USAGE, &self->memory_usage);
  stats_register_counter(1, &sc_key, SC_TYPE_WRITTEN, &self->written_messages);
  stats_unlock();

  log_queue_set_counters(self->queue, self->queued_messages,
                         self->dropped_messages, self->memory_usage);
  _update_memory_usage_counter_when_fifo_is_used(self);

  self->seq_num = GPOINTER_TO_INT(cfg_persist_config_fetch(cfg,
                                                           log_threaded_dest_driver_format_seqnum_for_persist(self)));
  if (!self->seq_num)
    init_sequence_number(&self->seq_num);

  log_threaded_dest_driver_start_thread(self);

  return TRUE;
}

gboolean
log_threaded_dest_driver_deinit_method(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;

  log_queue_reset_parallel_push(self->queue);

  log_queue_set_counters(self->queue, NULL, NULL, NULL);

  cfg_persist_config_add(log_pipe_get_config(s),
                         log_threaded_dest_driver_format_seqnum_for_persist(self),
                         GINT_TO_POINTER(self->seq_num), NULL, FALSE);

  save_counter_to_persistent_storage(log_pipe_get_config(s), self->memory_usage);

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

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

void
log_threaded_dest_driver_free(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;

  log_dest_driver_free((LogPipe *)self);
}

static void
log_threaded_dest_driver_queue(LogPipe *s, LogMessage *msg,
                               const LogPathOptions *path_options,
                               gpointer user_data)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  if (self->queue_method)
    self->queue_method(self);

  log_msg_add_ack(msg, path_options);
  log_queue_push_tail(self->queue, log_msg_ref(msg), path_options);

  stats_counter_inc(self->processed_messages);

  log_dest_driver_queue_method(s, msg, path_options, user_data);
}

void
log_threaded_dest_driver_init_instance(LogThrDestDriver *self, GlobalConfig *cfg)
{
  log_dest_driver_init_instance(&self->super, cfg);

  self->worker_options.is_output_thread = TRUE;

  self->super.super.super.init = log_threaded_dest_driver_start;
  self->super.super.super.deinit = log_threaded_dest_driver_deinit_method;
  self->super.super.super.queue = log_threaded_dest_driver_queue;
  self->super.super.super.free_fn = log_threaded_dest_driver_free;
  self->time_reopen = -1;

  self->retries.max = MAX_RETRIES_OF_FAILED_INSERT_DEFAULT;
}

void
log_threaded_dest_driver_message_accept(LogThrDestDriver *self,
                                        LogMessage *msg)
{
  self->retries.counter = 0;
  step_sequence_number(&self->seq_num);
  log_queue_ack_backlog(self->queue, 1);
  log_msg_unref(msg);
}

void
log_threaded_dest_driver_message_drop(LogThrDestDriver *self,
                                      LogMessage *msg)
{
  stats_counter_inc(self->dropped_messages);
  log_threaded_dest_driver_message_accept(self, msg);
}

void
log_threaded_dest_driver_message_rewind(LogThrDestDriver *self,
                                        LogMessage *msg)
{
  log_queue_rewind_backlog(self->queue, 1);
  log_msg_unref(msg);
}

void
log_threaded_dest_driver_set_max_retries(LogDriver *s, gint max_retries)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;

  self->retries.max = max_retries;
}
