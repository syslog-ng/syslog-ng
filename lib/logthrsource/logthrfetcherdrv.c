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

static EVTTAG *
evt_tag_driver(LogThreadedFetcherDriver *f)
{
  return evt_tag_str("driver", f->super.super.super.id);
}

static inline void
_thread_init(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher thread_init()", evt_tag_driver(self));
  if (self->thread_init)
    self->thread_init(self);
}

static inline void
_thread_deinit(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher thread_deinit()", evt_tag_driver(self));
  if (self->thread_deinit)
    self->thread_deinit(self);
}

static inline gboolean
_connect(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher connect()", evt_tag_driver(self));
  if (!self->connect)
    return TRUE;

  if (!self->connect(self))
    {
      msg_debug("Error establishing connection", evt_tag_driver(self));
      return FALSE;
    }

  return TRUE;
}

static inline void
_disconnect(LogThreadedFetcherDriver *self)
{
  msg_trace("Fetcher disconnect()", evt_tag_driver(self));
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
_worker_run(LogThreadedSourceDriver *s)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;

  iv_init();

  iv_event_register(&self->wakeup_event);
  iv_event_register(&self->shutdown_event);

  _thread_init(self);
  if (_connect(self))
    iv_task_register(&self->fetch_task);
  else
    _start_reconnect_timer(self);

  iv_main();

  _disconnect(self);
  _thread_deinit(self);

  iv_deinit();
}

static void
_worker_request_exit(LogThreadedSourceDriver *s)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;

  self->under_termination = TRUE;

  iv_event_post(&self->shutdown_event);

  if (self->request_exit)
    self->request_exit(self);
}

static void
_wakeup(LogThreadedSourceDriver *s)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;

  if (!self->under_termination)
    iv_event_post(&self->wakeup_event);
}

static inline void
_schedule_next_fetch_if_free_to_send(LogThreadedFetcherDriver *self)
{
  if (log_threaded_source_free_to_send(&self->super))
    iv_task_register(&self->fetch_task);
}

static void
_fetch(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  msg_trace("Fetcher fetch()", evt_tag_driver(self));

  LogThreadedFetchResult fetch_result = self->fetch(self);

  switch (fetch_result.result)
    {
    case THREADED_FETCH_ERROR:
      msg_error("Error during fetching messages", evt_tag_driver(self));
      _disconnect(self);
      _start_reconnect_timer(self);
      break;

    case THREADED_FETCH_NOT_CONNECTED:
      msg_info("Fetcher disconnected while receiving messages, reconnecting",
               evt_tag_driver(self));
      _start_reconnect_timer(self);
      break;

    case THREADED_FETCH_SUCCESS:
      log_threaded_source_post(&self->super, fetch_result.msg);
      _schedule_next_fetch_if_free_to_send(self);
      break;

    default:
      g_assert_not_reached();
    }
}

static void
_wakeup_event_handler(gpointer data)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) data;

  if (!iv_task_registered(&self->fetch_task))
    iv_task_register(&self->fetch_task);
}

static void
_stop_watches(LogThreadedFetcherDriver *self)
{
  iv_event_unregister(&self->wakeup_event);
  iv_event_unregister(&self->shutdown_event);

  if (iv_task_registered(&self->fetch_task))
    iv_task_unregister(&self->fetch_task);

  if (iv_timer_registered(&self->reconnect_timer))
    iv_timer_unregister(&self->reconnect_timer);
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
}

gboolean
log_threaded_fetcher_driver_init_method(LogPipe *s)
{
  LogThreadedFetcherDriver *self = (LogThreadedFetcherDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_threaded_source_driver_init_method(s))
    return FALSE;

  g_assert(self->fetch);

  if (cfg && self->time_reopen == -1)
    self->time_reopen = cfg->time_reopen;

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

  _init_watches(self);

  log_threaded_source_driver_set_worker_run(&self->super, _worker_run);
  log_threaded_source_driver_set_worker_request_exit(&self->super, _worker_request_exit);
  log_threaded_source_set_wakeup(&self->super, _wakeup);

  self->super.super.super.super.init = log_threaded_fetcher_driver_init_method;
  self->super.super.super.super.deinit = log_threaded_fetcher_driver_deinit_method;
  self->super.super.super.super.free_fn = log_threaded_fetcher_driver_free_method;
}
