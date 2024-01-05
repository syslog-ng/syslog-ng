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

#include "logthrfetcherdrv.h"
#include "messages.h"
#include "timeutils/misc.h"

#define SEC_TO_MSEC(x) ((x) * 1000)

void
log_threaded_fetcher_driver_set_fetch_no_data_delay(LogDriver *s, gdouble no_data_delay)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;
  self->no_data_delay = (gint64) SEC_TO_MSEC(no_data_delay);
}

void
log_threaded_fetcher_driver_set_time_reopen(LogDriver *s, time_t time_reopen)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;
  self->time_reopen = time_reopen;
}

static EVTTAG *
_tag_driver(LogThreadedFetcherDriver *f)
{
  return evt_tag_str("driver", f->super.super.super.id);
}


static inline gboolean
_connect(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher connect()", _tag_driver(self));
  if (!self->connect)
    return TRUE;

  if (!self->connect(self))
    {
      msg_debug("Error establishing connection", _tag_driver(self));
      return FALSE;
    }

  return TRUE;
}

static inline void
_disconnect(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher disconnect()", _tag_driver(self));
  if (self->disconnect)
    self->disconnect(self);
}

static void
_start_reconnect_timer(LogThreadedFetcherDriver *self)
{
  iv_validate_now();
  self->reconnect_timer.expires  = iv_now;
  self->reconnect_timer.expires.tv_sec += self->time_reopen;
  iv_timer_register(&self->reconnect_timer);
}

static void
_start_no_data_timer(LogThreadedFetcherDriver *self)
{
  iv_validate_now();
  self->no_data_timer.expires  = iv_now;
  timespec_add_msec(&self->no_data_timer.expires, self->no_data_delay);
  iv_timer_register(&self->no_data_timer);
}

static gboolean
_worker_thread_init(LogThreadedSourceWorker *w)
{
  LogThreadedFetcherDriver *control = (LogThreadedFetcherDriver *) w->control;

  iv_event_register(&control->shutdown_event);

  msg_trace("Fetcher thread_init()", _tag_driver(control));
  if (control->thread_init)
    control->thread_init(control);
  return TRUE;
}

static void
_worker_thread_deinit(LogThreadedSourceWorker *w)
{
  LogThreadedFetcherDriver *control = (LogThreadedFetcherDriver *) w->control;

  msg_trace("Fetcher thread_deinit()", _tag_driver(control));
  if (control->thread_deinit)
    control->thread_deinit(control);
  iv_event_unregister(&control->shutdown_event);
}

static void
_worker_run(LogThreadedSourceWorker *w)
{
  LogThreadedFetcherDriver *control = (LogThreadedFetcherDriver *) w->control;

  iv_event_register(&control->wakeup_event);
  if (_connect(control))
    iv_task_register(&control->fetch_task);
  else
    _start_reconnect_timer(control);

  iv_main();

  _disconnect(control);
}

static void
_worker_request_exit(LogThreadedSourceWorker *w)
{
  LogThreadedFetcherDriver *control = (LogThreadedFetcherDriver *) w->control;

  control->under_termination = TRUE;

  iv_event_post(&control->shutdown_event);

  if (control->request_exit)
    control->request_exit(control);
}

static void
_worker_wakeup(LogSource *s)
{
  LogThreadedSourceWorker *self = (LogThreadedSourceWorker *) s;
  LogThreadedFetcherDriver *control = (LogThreadedFetcherDriver *) self->control;

  if (!control->under_termination)
    iv_event_post(&control->wakeup_event);
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  /* LogThreadedFetcherDriver uses the multi-worker API, but it is not prepared to work with more than one worker. */
  g_assert(s->num_workers == 1);

  LogThreadedSourceWorker *worker = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(worker, s, worker_index);

  worker->super.wakeup = _worker_wakeup;
  worker->thread_init = _worker_thread_init;
  worker->thread_deinit = _worker_thread_deinit;
  worker->run = _worker_run;
  worker->request_exit = _worker_request_exit;

  return worker;
}

static inline void
_schedule_next_fetch_if_free_to_send(LogThreadedFetcherDriver *self)
{
  if (log_threaded_source_worker_free_to_send(self->super.workers[0]))
    iv_task_register(&self->fetch_task);
  else
    self->suspended = TRUE;
}

static void
_on_fetch_error(LogThreadedFetcherDriver *self)
{
  msg_error("Error during fetching messages", _tag_driver(self));
  _disconnect(self);
  _start_reconnect_timer(self);
}

static void
_on_not_connected(LogThreadedFetcherDriver *self)
{
  msg_info("Fetcher disconnected while receiving messages, reconnecting", _tag_driver(self));
  _start_reconnect_timer(self);
}

static void
_on_fetch_success(LogThreadedFetcherDriver *self, LogMessage *msg)
{
  log_threaded_source_worker_post(self->super.workers[0], msg);
  _schedule_next_fetch_if_free_to_send(self);
}

static void
_on_fetch_try_again(LogThreadedFetcherDriver *self)
{
  msg_debug("Try again when fetching messages", _tag_driver(self));
  iv_task_register(&self->fetch_task);
}

static void
_on_fetch_no_data(LogThreadedFetcherDriver *self)
{
  msg_debug("No data during fetching messages", _tag_driver(self));
  _start_no_data_timer(self);
}


static void
_fetch(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  msg_trace("Fetcher fetch()", _tag_driver(self));

  LogThreadedFetchResult fetch_result = self->fetch(self);

  switch (fetch_result.result)
    {
    case THREADED_FETCH_ERROR:
      _on_fetch_error(self);
      break;

    case THREADED_FETCH_NOT_CONNECTED:
      _on_not_connected(self);
      break;

    case THREADED_FETCH_SUCCESS:
      _on_fetch_success(self, fetch_result.msg);
      break;

    case THREADED_FETCH_TRY_AGAIN:
      _on_fetch_try_again(self);
      break;

    case THREADED_FETCH_NO_DATA:
      _on_fetch_no_data(self);
      break;

    default:
      g_assert_not_reached();
    }
}

static void
_wakeup_event_handler(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  if (self->suspended && log_threaded_source_worker_free_to_send(self->super.workers[0]))
    {
      self->suspended = FALSE;

      if (!iv_task_registered(&self->fetch_task))
        iv_task_register(&self->fetch_task);
    }
}

static void
_stop_watches(LogThreadedFetcherDriver *self)
{
  iv_event_unregister(&self->wakeup_event);

  if (iv_task_registered(&self->fetch_task))
    iv_task_unregister(&self->fetch_task);

  if (iv_timer_registered(&self->reconnect_timer))
    iv_timer_unregister(&self->reconnect_timer);

  if (iv_timer_registered(&self->no_data_timer))
    iv_timer_unregister(&self->no_data_timer);
}

static void
_shutdown_event_handler(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  _stop_watches(self);

  iv_quit();
}

static void
_reconnect(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  if (_connect(self))
    _schedule_next_fetch_if_free_to_send(self);
  else
    _start_reconnect_timer(self);
}

static void
_no_data(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  iv_task_register(&self->fetch_task);
}

static void
_init_watches(LogThreadedFetcherDriver *self)
{
  IV_TASK_INIT(&self->fetch_task);
  self->fetch_task.cookie = self;
  self->fetch_task.handler = _fetch;

  IV_EVENT_INIT(&self->wakeup_event);
  self->wakeup_event.cookie = self;
  self->wakeup_event.handler = _wakeup_event_handler;

  IV_EVENT_INIT(&self->shutdown_event);
  self->shutdown_event.cookie = self;
  self->shutdown_event.handler = _shutdown_event_handler;

  IV_TIMER_INIT(&self->reconnect_timer);
  self->reconnect_timer.cookie = self;
  self->reconnect_timer.handler = _reconnect;

  IV_TIMER_INIT(&self->no_data_timer);
  self->no_data_timer.cookie = self;
  self->no_data_timer.handler = _no_data;

}

gboolean
log_threaded_fetcher_driver_init_method(LogPipe *s)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_threaded_source_driver_init_method(s))
    return FALSE;


  g_assert(self->fetch);

  if (self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

  if (self->no_data_delay == -1)
    log_threaded_fetcher_driver_set_fetch_no_data_delay(&self->super.super.super, cfg->time_reopen);

  return TRUE;
}

gboolean
log_threaded_fetcher_driver_deinit_method(LogPipe *s)
{
  return log_threaded_source_driver_deinit_method(s);
}

void
log_threaded_fetcher_driver_free_method(LogPipe *s)
{
  log_threaded_source_driver_free_method(s);
}

void
log_threaded_fetcher_driver_init_instance(LogThreadedFetcherDriver *self, GlobalConfig *cfg)
{
  log_threaded_source_driver_init_instance(&self->super, cfg);

  self->time_reopen = -1;
  self->no_data_delay = -1;

  _init_watches(self);

  self->super.super.super.super.init = log_threaded_fetcher_driver_init_method;
  self->super.super.super.super.deinit = log_threaded_fetcher_driver_deinit_method;
  self->super.super.super.super.free_fn = log_threaded_fetcher_driver_free_method;

  self->super.worker_construct = _construct_worker;
}
