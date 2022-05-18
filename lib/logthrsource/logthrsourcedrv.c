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
log_threaded_source_worker_logpipe(LogThreadedSourceWorker *self)
{
  return &self->super.super;
}

static void
log_threaded_source_worker_set_options(LogThreadedSourceWorker *self, LogThreadedSourceDriver *control,
                                       LogThreadedSourceWorkerOptions *options,
                                       const gchar *stats_id, const gchar *stats_instance)
{
  log_source_set_options(&self->super, &options->super, stats_id, stats_instance, TRUE,
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
                                        const gchar *group_name)
{
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
log_threaded_source_suspend(LogThreadedSourceDriver *self)
{
  LogThreadedSourceWorker *worker = self->worker;

  while (!log_threaded_source_free_to_send(self) && !worker->under_termination)
    wakeup_cond_wait(&worker->wakeup_cond);
}

static void
log_threaded_source_wakeup(LogThreadedSourceDriver *self)
{
  LogThreadedSourceWorker *worker = self->worker;

  wakeup_cond_signal(&worker->wakeup_cond);
}

static gboolean
log_threaded_source_worker_thread_init(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  if (self->control->thread_init)
    return self->control->thread_init(self->control);
  return TRUE;
}

static void
log_threaded_source_worker_thread_deinit(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  if (self->control->thread_deinit)
    self->control->thread_deinit(self->control);
}

static void
log_threaded_source_worker_run(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->control->super.super.id));

  self->control->run(self->control);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->control->super.super.id));
}

static void
log_threaded_source_worker_request_exit(MainLoopThreadedWorker *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s->data;

  msg_debug("Requesting worker thread exit",
            evt_tag_str("driver", self->control->super.super.id));
  self->under_termination = TRUE;
  self->control->request_exit(self->control);
  log_threaded_source_wakeup(self->control);
}

static void
_worker_wakeup(LogSource *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s;

  self->control->wakeup(self->control);
}

static gboolean
log_threaded_source_worker_init(LogPipe *s)
{
  if (!log_source_init(s))
    return FALSE;

  return TRUE;
}

static void
log_threaded_source_worker_free(LogPipe *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s;

  wakeup_cond_destroy(&self->wakeup_cond);

  log_pipe_unref(&self->control->super.super.super);
  self->control = NULL;

  main_loop_threaded_worker_clear(&self->thread);
  log_source_free(s);
}

static LogThreadedSourceWorker *
log_threaded_source_worker_new(GlobalConfig *cfg)
{
  LogThreadedSourceWorker *self = g_new0(LogThreadedSourceWorker, 1);
  log_source_init_instance(&self->super, cfg);
  main_loop_threaded_worker_init(&self->thread, MLW_THREADED_INPUT_WORKER, self);
  self->thread.thread_init = log_threaded_source_worker_thread_init;
  self->thread.thread_deinit = log_threaded_source_worker_thread_deinit;
  self->thread.run = log_threaded_source_worker_run;
  self->thread.request_exit = log_threaded_source_worker_request_exit;

  wakeup_cond_init(&self->wakeup_cond);

  self->super.super.init = log_threaded_source_worker_init;
  self->super.super.free_fn = log_threaded_source_worker_free;
  self->super.wakeup = _worker_wakeup;

  return self;
}

gboolean
log_threaded_source_driver_init_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  self->worker = log_threaded_source_worker_new(cfg);

  if (!log_src_driver_init_method(s))
    return FALSE;

  g_assert(self->format_stats_instance);

  log_threaded_source_worker_options_init(&self->worker_options, cfg, self->super.super.group);
  log_threaded_source_worker_set_options(self->worker, self, &self->worker_options,
                                         self->super.super.id, self->format_stats_instance(self));

  LogPipe *worker_pipe = log_threaded_source_worker_logpipe(self->worker);
  log_pipe_append(worker_pipe, s);
  if (!log_pipe_init(worker_pipe))
    {
      log_pipe_unref(worker_pipe);
      self->worker = NULL;
      return FALSE;
    }

  return TRUE;
}

gboolean
log_threaded_source_driver_deinit_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;
  LogPipe *worker_pipe = log_threaded_source_worker_logpipe(self->worker);

  log_pipe_deinit(worker_pipe);
  log_pipe_unref(worker_pipe);

  return log_src_driver_deinit_method(s);
}

void
log_threaded_source_driver_free_method(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  log_threaded_source_worker_options_destroy(&self->worker_options);

  log_src_driver_free(s);
}

gboolean
log_threaded_source_driver_start_worker(LogPipe *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  main_loop_threaded_worker_start(&self->worker->thread);

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

void
log_threaded_source_post(LogThreadedSourceDriver *self, LogMessage *msg)
{
  msg_debug("Incoming log message",
            evt_tag_str("input", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
            evt_tag_msg_reference(msg));
  _apply_default_priority_and_facility(self, msg);
  log_source_post(&self->worker->super, msg);
}

gboolean
log_threaded_source_free_to_send(LogThreadedSourceDriver *self)
{
  return log_source_free_to_send(&self->worker->super);
}

void
log_threaded_source_blocking_post(LogThreadedSourceDriver *self, LogMessage *msg)
{
  LogThreadedSourceWorker *worker = self->worker;

  log_threaded_source_post(self, msg);

  /*
   * The wakeup lock must be held before calling free_to_send() and suspend(),
   * otherwise g_cond_signal() might be called between free_to_send() and
   * suspend(). We'd hang in that case.
   *
   * LogReader does not have such a lock, but this is because it runs an ivykis
   * loop with a _synchronized_ event queue, where suspend() and the
   * "schedule_wakeup" event are guaranteed to be scheduled in the right order.
   */

  wakeup_cond_lock(&worker->wakeup_cond);
  if (!log_threaded_source_free_to_send(self))
    log_threaded_source_suspend(self);
  wakeup_cond_unlock(&worker->wakeup_cond);
}

void
log_threaded_source_driver_init_instance(LogThreadedSourceDriver *self, GlobalConfig *cfg)
{
  log_src_driver_init_instance(&self->super, cfg);

  log_threaded_source_worker_options_defaults(&self->worker_options);

  self->super.super.super.init = log_threaded_source_driver_init_method;
  self->super.super.super.deinit = log_threaded_source_driver_deinit_method;
  self->super.super.super.free_fn = log_threaded_source_driver_free_method;
  self->super.super.super.on_config_inited = log_threaded_source_driver_start_worker;

  self->wakeup = log_threaded_source_wakeup;
}
