/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#include "logthrdestdrv.h"
#include "misc.h"

void
log_threaded_dest_driver_suspend(LogThrDestDriver *self)
{
  self->writer_thread_suspended = TRUE;
  g_get_current_time(&self->writer_thread_suspend_target);
  g_time_val_add(&self->writer_thread_suspend_target,
                 self->time_reopen * 1000000);
}

static void
log_threaded_dest_driver_message_became_available_in_the_queue(gpointer user_data)
{
  LogThrDestDriver *self = (LogThrDestDriver *) user_data;

  g_mutex_lock(self->suspend_mutex);
  g_cond_signal(self->writer_thread_wakeup_cond);
  g_mutex_unlock(self->suspend_mutex);
}

static gpointer
log_threaded_dest_driver_worker_thread_main(gpointer arg)
{
  LogThrDestDriver *self = (LogThrDestDriver *)arg;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.id),
            NULL);

  if (self->worker.thread_init)
    self->worker.thread_init(self);

  while (!self->writer_thread_terminate)
    {
      g_mutex_lock(self->suspend_mutex);
      if (self->writer_thread_suspended)
        {
          g_cond_timed_wait(self->writer_thread_wakeup_cond,
                            self->suspend_mutex,
                            &self->writer_thread_suspend_target);
          self->writer_thread_suspended = FALSE;
          g_mutex_unlock(self->suspend_mutex);
        }
      else if (!log_queue_check_items(self->queue, NULL,
                                      log_threaded_dest_driver_message_became_available_in_the_queue,
                                      self, NULL))
        {
          g_cond_wait(self->writer_thread_wakeup_cond, self->suspend_mutex);
          g_mutex_unlock(self->suspend_mutex);
        }
      else
        g_mutex_unlock(self->suspend_mutex);

      if (self->writer_thread_terminate)
        break;

      if (!self->worker.insert(self))
        {
          if (self->worker.disconnect)
            self->worker.disconnect(self);
          log_threaded_dest_driver_suspend(self);
        }
    }

  if (self->worker.disconnect)
    self->worker.disconnect(self);

  if (self->worker.thread_deinit)
    self->worker.thread_deinit(self);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->super.super.id),
            NULL);

  return NULL;
}

static void
log_threaded_dest_driver_start_thread(LogThrDestDriver *self)
{
  self->writer_thread = create_worker_thread(log_threaded_dest_driver_worker_thread_main,
                                             self, TRUE, NULL);
}

static void
log_threaded_dest_driver_stop_thread(LogThrDestDriver *self)
{
  self->writer_thread_terminate = TRUE;
  g_mutex_lock(self->suspend_mutex);
  g_cond_signal(self->writer_thread_wakeup_cond);
  g_mutex_unlock(self->suspend_mutex);
  g_thread_join(self->writer_thread);
}

gboolean
log_threaded_dest_driver_start(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  self->queue = log_dest_driver_acquire_queue(&self->super,
                                              self->format.persist_name(self));

  if (self->queue == NULL)
    {
      return FALSE;
    }

  stats_lock();
  stats_register_counter(0, self->stats_source | SCS_DESTINATION, self->super.super.id,
                         self->format.stats_instance(self),
                         SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, self->stats_source | SCS_DESTINATION, self->super.super.id,
                         self->format.stats_instance(self),
                         SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  log_queue_set_counters(self->queue, self->stored_messages,
                         self->dropped_messages);

  log_threaded_dest_driver_start_thread(self);

  return TRUE;
}

gboolean
log_threaded_dest_driver_deinit_method(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;

  log_threaded_dest_driver_stop_thread(self);
  log_queue_reset_parallel_push(self->queue);

  log_queue_set_counters(self->queue, NULL, NULL);

  stats_lock();
  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->super.super.id,
                           self->format.stats_instance(self),
                           SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(self->stats_source | SCS_DESTINATION, self->super.super.id,
                           self->format.stats_instance(self),
                           SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

void
log_threaded_dest_driver_free(LogPipe *s)
{
  LogThrDestDriver *self = (LogThrDestDriver *)s;

  g_mutex_free(self->suspend_mutex);
  g_cond_free(self->writer_thread_wakeup_cond);

  if (self->queue)
    log_queue_unref(self->queue);

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

  log_dest_driver_queue_method(s, msg, path_options, user_data);
}

void
log_threaded_dest_driver_init_instance(LogThrDestDriver *self)
{
  log_dest_driver_init_instance(&self->super);

  self->writer_thread_wakeup_cond = g_cond_new();
  self->suspend_mutex = g_mutex_new();

  self->super.super.super.init = log_threaded_dest_driver_start;
  self->super.super.super.deinit = log_threaded_dest_driver_deinit_method;
  self->super.super.super.queue = log_threaded_dest_driver_queue;
  self->super.super.super.free_fn = log_threaded_dest_driver_free;
}
