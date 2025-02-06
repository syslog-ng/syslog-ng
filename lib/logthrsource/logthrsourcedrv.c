/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#include "logthrsourcedrv.h"
#include "mainloop-threaded-worker.h"
#include "messages.h"
#include "apphook.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/stats-cluster-key-builder.h"

#include <iv.h>


static void
wakeup_cond_init(WakeupCondition *cond)
{
  g_mutex_init(&cond->lock);
  g_cond_init(&cond->cond);
  cond->awoken = TRUE;
}

static void
wakeup_cond_destroy(WakeupCondition *cond)
{
  g_cond_clear(&cond->cond);
  g_mutex_clear(&cond->lock);
}

static inline void
wakeup_cond_lock(WakeupCondition *cond)
{
  g_mutex_lock(&cond->lock);
}

static inline void
wakeup_cond_unlock(WakeupCondition *cond)
{
  g_mutex_unlock(&cond->lock);
}

/* The wakeup lock must be held before calling this function. */
static inline void
wakeup_cond_wait(WakeupCondition *cond)
{
  cond->awoken = FALSE;
  while (!cond->awoken)
    g_cond_wait(&cond->cond, &cond->lock);
}

static inline void
wakeup_cond_signal(WakeupCondition *cond)
{
  g_mutex_lock(&cond->lock);
  cond->awoken = TRUE;
  g_cond_signal(&cond->cond);
  g_mutex_unlock(&cond->lock);
}

static LogPipe *
_worker_logpipe(LogThreadedSourceWorker *self)
{
  return &self->super.super;
}

static void
_worker_set_options(LogThreadedSourceWorker *self, LogThreadedSourceDriver *control,
                    LogThreadedSourceWorkerOptions *options,
                    const gchar *stats_id, StatsClusterKeyBuilder *kb)
{
  log_source_set_options(&self->super, &options->super, stats_id, kb, TRUE,
                         control->super.super.super.expr_node);
  log_source_set_ack_tracker_factory(&self->super, ack_tracker_factory_ref(options->ack_tracker_factory));

  log_pipe_unref(&self->control->super.super.super);
  log_pipe_ref(&control->super.super.super);
  self->control = control;
}

void
log_threaded_source_worker_options_defaults(LogThreadedSourceWorkerOptions *options)
{
  log_source_options_defaults(&options->super);
  msg_format_options_defaults(&options->parse_options);
  options->parse_options.flags |= LP_SYSLOG_PROTOCOL;
  options->ack_tracker_factory = NULL;
}

void
log_threaded_source_worker_options_init(LogThreadedSourceWorkerOptions *options, GlobalConfig *cfg,
                                        const gchar *group_name, gint num_workers)
{
  if (options->super.init_window_size == -1)
    {
      options->super.init_window_size = 100 * num_workers;
    }

  options->super.init_window_size /= num_workers;

  log_source_options_init(&options->super, cfg, group_name);
  msg_format_options_init(&options->parse_options, cfg);
}

void
log_threaded_source_worker_options_destroy(LogThreadedSourceWorkerOptions *options)
{
  log_source_options_destroy(&options->super);
  msg_format_options_destroy(&options->parse_options);
  ack_tracker_factory_unref(options->ack_tracker_factory);
}

/* The wakeup lock must be held before calling this function. */
static void
_worker_suspend(LogThreadedSourceWorker *self)
{
  while (!log_threaded_source_worker_free_to_send(self) && !self->under_termination)
    wakeup_cond_wait(&self->wakeup_cond);
}

static gboolean
_worker_thread_init(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  if (self->thread_init)
    return self->thread_init(self);
  return TRUE;
}

static void
_worker_thread_deinit(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  if (self->thread_deinit)
    self->thread_deinit(self);
}

static void
_worker_thread_run(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->control->super.super.id),
            evt_tag_int("worker_index", self->worker_index));

  self->run(self);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->control->super.super.id),
            evt_tag_int("worker_index", self->worker_index));
}

static void
_worker_wakeup(LogSource *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s;

  wakeup_cond_signal(&self->wakeup_cond);

  if (self->wakeup)
    self->wakeup(self);
}

static void
_worker_thread_request_exit(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  msg_debug("Requesting worker thread exit",
            evt_tag_str("driver", self->control->super.super.id),
            evt_tag_int("worker_index", self->worker_index));
  self->under_termination = TRUE;
  self->request_exit(self);
  _worker_wakeup(&self->super);
}

static gboolean
_worker_init(LogPipe *s)
{
  if (!log_source_init(s))
    return FALSE;

  return TRUE;
}

void
log_threaded_source_worker_free(LogPipe *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s;

  wakeup_cond_destroy(&self->wakeup_cond);

  log_pipe_unref(&self->control->super.super.super);
  self->control = NULL;

  main_loop_threaded_worker_clear(&self->thread);
  log_source_free(s);
}

void
log_threaded_source_worker_init_instance(LogThreadedSourceWorker *self, LogThreadedSourceDriver *driver,
                                         gint worker_index)
{
  log_source_init_instance(&self->super, log_pipe_get_config(&driver->super.super.super));
  main_loop_threaded_worker_init(&self->thread, MLW_THREADED_INPUT_WORKER, self);
  self->thread.thread_init = _worker_thread_init;
  self->thread.thread_deinit = _worker_thread_deinit;
  self->thread.run = _worker_thread_run;
  self->thread.request_exit = _worker_thread_request_exit;

  wakeup_cond_init(&self->wakeup_cond);

  self->super.super.init = _worker_init;
  self->super.super.free_fn = log_threaded_source_worker_free;
  self->super.wakeup = _worker_wakeup;

  self->super.metrics.raw_bytes_enabled = driver->raw_bytes_metrics_enabled;

  self->worker_index = worker_index;
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *self, gint worker_index)
{
  LogThreadedSourceWorker *worker = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(worker, self, worker_index);
  return worker;
}

gboolean
log_threaded_source_driver_pre_config_init(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;
  main_loop_worker_allocate_thread_space(self->num_workers);
  return TRUE;
}

static void
_create_workers(LogThreadedSourceDriver *self)
{
  g_assert(!self->workers);

  self->workers = g_new0(LogThreadedSourceWorker *, self->num_workers);
  for (size_t i = 0; i < self->num_workers; i++)
    {
      self->workers[i] = self->worker_construct(self, i);
    }
}

static void
_destroy_workers(LogThreadedSourceDriver *self)
{
  for (size_t i = 0; i < self->num_workers; i++)
    {
      LogPipe *worker_pipe = _worker_logpipe(self->workers[i]);
      if (!worker_pipe)
        break;

      log_pipe_deinit(worker_pipe);
      log_pipe_unref(worker_pipe);
      self->workers[i] = NULL;
    }

  g_free(self->workers);
  self->workers = NULL;
}

static gboolean
_init_workers(LogThreadedSourceDriver *self)
{
  g_assert(self->format_stats_key);

  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  log_threaded_source_worker_options_init(&self->worker_options, cfg, self->super.super.group, self->num_workers);

  for (size_t i = 0; i < self->num_workers; i++)
    {
      StatsClusterKeyBuilder *kb = stats_cluster_key_builder_new();
      self->format_stats_key(self, kb);
      _worker_set_options(self->workers[i], self,
                          &self->worker_options, self->super.super.id, kb);

      LogPipe *worker_pipe = _worker_logpipe(self->workers[i]);
      log_pipe_append(worker_pipe, &self->super.super.super);
      if (!log_pipe_init(worker_pipe))
        return FALSE;
    }

  return TRUE;
}

gboolean
log_threaded_source_driver_init_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  _create_workers(self);

  if (!log_src_driver_init_method(s))
    goto error;

  if (!_init_workers(self))
    goto error;

  return TRUE;

error:
  _destroy_workers(self);
  return FALSE;
}

gboolean
log_threaded_source_driver_deinit_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  _destroy_workers(self);

  return log_src_driver_deinit_method(s);
}

void
log_threaded_source_driver_free_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  g_free(self->transport_name);
  log_threaded_source_worker_options_destroy(&self->worker_options);

  log_src_driver_free(s);
}

gboolean
log_threaded_source_driver_start_workers(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  for (size_t i = 0; i < self->num_workers; i++)
    g_assert(main_loop_threaded_worker_start(&self->workers[i]->thread));

  return TRUE;
}

static gboolean
_is_default_priority_or_facility_set(MsgFormatOptions *parse_options)
{
  return parse_options->default_pri != 0xFFFF;
}

static void
_apply_default_priority_and_facility(LogThreadedSourceDriver *self, LogMessage *msg)
{
  MsgFormatOptions *parse_options = &self->worker_options.parse_options;
  if (!_is_default_priority_or_facility_set(parse_options))
    return;
  msg->pri = parse_options->default_pri;
}

static void
_apply_message_attributes(LogThreadedSourceDriver *self, LogMessage *msg)
{
  _apply_default_priority_and_facility(self, msg);
  if (self->transport_name)
    log_msg_set_value(msg, LM_V_TRANSPORT, self->transport_name, self->transport_name_len);
}

/*
 * Call this every some messages so consumers that accumulate multiple
 * messages (LogQueueFifo for instance) can finish accumulation and go on
 * processing a batch.
 *
 * Basically this calls main_loop_worker_invoke_batch_callbacks(), which is
 * done by the minaloop-io-worker layer whenever we go back to the main
 * loop.  Whether this is done automatically by LogThreadedSourceWorker is
 * controlled by the auto_close_batches member, in which case we do this
 * every message.
 *
 * Doing it every message defeats the purpose more or less, as consumers
 * tend to do batching to improve performance.
 */
void
log_threaded_source_worker_close_batch(LogThreadedSourceWorker *self)
{
  main_loop_worker_invoke_batch_callbacks();
}

void
log_threaded_source_worker_post(LogThreadedSourceWorker *self, LogMessage *msg)
{
  msg_debug("Incoming log message",
            evt_tag_str("input", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
            evt_tag_str("driver", self->control->super.super.id),
            evt_tag_int("worker_index", self->worker_index),
            evt_tag_msg_reference(msg));
  _apply_message_attributes(self->control, msg);
  log_source_post(&self->super, msg);

  if (self->control->auto_close_batches)
    log_threaded_source_worker_close_batch(self);
}

gboolean
log_threaded_source_worker_free_to_send(LogThreadedSourceWorker *self)
{
  return log_source_free_to_send(&self->super);
}

void
log_threaded_source_worker_blocking_post(LogThreadedSourceWorker *self, LogMessage *msg)
{
  log_threaded_source_worker_post(self, msg);

  /* unlocked, as only this thread can decrease the window size */
  if (!self->control->auto_close_batches && !log_threaded_source_worker_free_to_send(self))
    log_threaded_source_worker_close_batch(self);

  /*
   * The wakeup lock must be held before calling free_to_send() and suspend(),
   * otherwise g_cond_signal() might be called between free_to_send() and
   * suspend(). We'd hang in that case.
   *
   * LogReader does not have such a lock, but this is because it runs an ivykis
   * loop with a _synchronized_ event queue, where suspend() and the
   * "schedule_wakeup" event are guaranteed to be scheduled in the right order.
   */

  wakeup_cond_lock(&self->wakeup_cond);
  if (!log_threaded_source_worker_free_to_send(self))
    _worker_suspend(self);
  wakeup_cond_unlock(&self->wakeup_cond);
}

void
log_threaded_source_driver_set_transport_name(LogThreadedSourceDriver *self, const gchar *transport_name)
{
  g_free(self->transport_name);
  self->transport_name = g_strdup(transport_name);
  self->transport_name_len = strlen(transport_name);
}

void
log_threaded_source_driver_init_instance(LogThreadedSourceDriver *self, GlobalConfig *cfg)
{
  log_src_driver_init_instance(&self->super, cfg);

  log_threaded_source_worker_options_defaults(&self->worker_options);

  self->super.super.super.init = log_threaded_source_driver_init_method;
  self->super.super.super.deinit = log_threaded_source_driver_deinit_method;
  self->super.super.super.free_fn = log_threaded_source_driver_free_method;
  self->super.super.super.pre_config_init = log_threaded_source_driver_pre_config_init;
  self->super.super.super.post_config_init = log_threaded_source_driver_start_workers;

  self->worker_construct = _construct_worker;

  self->auto_close_batches = TRUE;
  self->num_workers = 1;
}
