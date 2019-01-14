/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "logreader.h"
#include "mainloop-io-worker.h"
#include "mainloop-call.h"
#include "ack_tracker.h"
#include "scratch-buffers.h"

#include <iv_event.h>

struct _LogReader
{
  LogSource super;
  LogProtoServer *proto;
  gboolean immediate_check;
  LogPipe *control;
  LogReaderOptions *options;
  PollEvents *poll_events;
  GSockAddr *peer_addr;

  /* NOTE: these used to be LogReaderWatch members, which were merged into
   * LogReader with the multi-thread refactorization */

  struct iv_task restart_task;
  struct iv_event schedule_wakeup;
  struct iv_event last_msg_sent_event;
  MainLoopIOWorkerJob io_job;
  gboolean watches_running:1, suspended:1;
  gint notify_code;


  /* proto & poll_events pending to be applied. As long as the previous
   * processing is being done, we can't replace these in self->proto and
   * self->poll_events, they get applied to the production ones as soon as
   * the previous work is finished */
  gboolean pending_proto_present;
  GCond *pending_proto_cond;
  GStaticMutex pending_proto_lock;
  LogProtoServer *pending_proto;
  PollEvents *pending_poll_events;

  struct iv_timer idle_timer;
};

static gboolean log_reader_fetch_log(LogReader *self);

static void log_reader_stop_watches(LogReader *self);
static void log_reader_stop_idle_timer(LogReader *self);
static void log_reader_idle_timeout(void *cookie);

static void log_reader_update_watches(LogReader *self);

static void log_reader_wakeup(LogSource *s);

static void
log_reader_apply_proto_and_poll_events(LogReader *self, LogProtoServer *proto, PollEvents *poll_events)
{
  if (self->proto)
    log_proto_server_free(self->proto);
  if (self->poll_events)
    poll_events_free(self->poll_events);

  self->proto = proto;

  if (self->proto)
    log_proto_server_set_wakeup_cb(self->proto, (LogProtoServerWakeupFunc) log_reader_wakeup, self);

  self->poll_events = poll_events;
}

static void
_last_msg_sent(gpointer s)
{
  LogReader *self = (LogReader *) s;
  log_pipe_notify(self->control, NC_LAST_MSG_SENT, self);
}

static void
log_reader_work_perform(void *s)
{
  LogReader *self = (LogReader *) s;

  self->notify_code = log_reader_fetch_log(self);
}

static void
log_reader_work_finished(void *s)
{
  LogReader *self = (LogReader *) s;

  if (self->pending_proto_present)
    {
      /* pending proto is only set in the main thread, so no need to
       * lock it before coming here. After we're syncing with the
       * log_writer_reopen() call, quite possibly coming from a
       * non-main thread. */

      g_static_mutex_lock(&self->pending_proto_lock);

      log_reader_apply_proto_and_poll_events(self, self->pending_proto, self->pending_poll_events);
      self->pending_proto = NULL;
      self->pending_poll_events = NULL;
      self->pending_proto_present = FALSE;

      g_cond_signal(self->pending_proto_cond);
      g_static_mutex_unlock(&self->pending_proto_lock);
    }

  if (self->notify_code)
    {
      gint notify_code = self->notify_code;

      self->notify_code = 0;
      log_pipe_notify(self->control, notify_code, self);
    }
  if (self->super.super.flags & PIF_INITIALIZED)
    {
      /* reenable polling the source assuming that we're still in
       * business (e.g. the reader hasn't been uninitialized) */

      log_proto_server_reset_error(self->proto);
      log_reader_update_watches(self);
    }
}

static void
log_reader_wakeup_triggered(gpointer s)
{
  LogReader *self = (LogReader *) s;

  if (!self->io_job.working && self->suspended)
    {
      /* NOTE: by the time working is set to FALSE we're over an
       * update_watches call.  So it is called either here (when
       * work_finished has done its work) or from work_finished above. The
       * two are not racing as both run in the main thread
       */
      log_reader_update_watches(self);
    }
}

/* NOTE: may be running in the destination's thread, thus proper locking must be used */
static void
log_reader_wakeup(LogSource *s)
{
  LogReader *self = (LogReader *) s;

  /*
   * We might get called even after this LogReader has been
   * deinitialized, in which case we must not do anything (since the
   * iv_event triggered here is not registered).
   *
   * This happens when log_writer_deinit() flushes its output queue
   * after the reader which produced the message has already been
   * deinited. Since init/deinit calls are made in the main thread, no
   * locking is needed.
   *
   */

  if (self->super.super.flags & PIF_INITIALIZED)
    iv_event_post(&self->schedule_wakeup);
}

static void
log_reader_window_empty(LogSource *s)
{
  LogReader *self = (LogReader *) s;
  if (self->super.super.flags & PIF_INITIALIZED)
    iv_event_post(&self->last_msg_sent_event);
}

static void
log_reader_io_process_input(gpointer s)
{
  LogReader *self = (LogReader *) s;

  log_reader_stop_watches(self);
  if ((self->options->flags & LR_THREADED))
    {
      main_loop_io_worker_job_submit(&self->io_job);
    }
  else
    {
      /* Checking main_loop_io_worker_job_quit() helps to speed up the
       * reload process.  If reload/shutdown is requested we shouldn't do
       * anything here, outstanding messages will be processed by the new
       * configuration.
       *
       * Our current understanding is that it doesn't prevent race
       * conditions of any kind.
       */
      if (!main_loop_worker_job_quit())
        {
          log_reader_work_perform(s);
          log_reader_work_finished(s);
        }
    }
}

static void
log_reader_init_watches(LogReader *self)
{
  IV_TASK_INIT(&self->restart_task);
  self->restart_task.cookie = self;
  self->restart_task.handler = log_reader_io_process_input;

  IV_EVENT_INIT(&self->schedule_wakeup);
  self->schedule_wakeup.cookie = self;
  self->schedule_wakeup.handler = log_reader_wakeup_triggered;

  IV_EVENT_INIT(&self->last_msg_sent_event);
  self->last_msg_sent_event.cookie = self;
  self->last_msg_sent_event.handler = _last_msg_sent;

  IV_TIMER_INIT(&self->idle_timer);
  self->idle_timer.cookie = self;
  self->idle_timer.handler = log_reader_idle_timeout;

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = (void (*)(void *)) log_reader_work_perform;
  self->io_job.completion = (void (*)(void *)) log_reader_work_finished;
  self->io_job.engage = (void (*)(void *)) log_pipe_ref;
  self->io_job.release = (void (*)(void *)) log_pipe_unref;
}

static void
log_reader_stop_watches(LogReader *self)
{
  if (self->watches_running)
    {
      poll_events_stop_watches(self->poll_events);

      if (iv_task_registered(&self->restart_task))
        iv_task_unregister(&self->restart_task);
      self->watches_running = FALSE;
    }
}

static void
log_reader_stop_idle_timer(LogReader *self)
{
  if (iv_timer_registered(&self->idle_timer))
    iv_timer_unregister(&self->idle_timer);
}

static void
log_reader_start_watches_if_stopped(LogReader *self)
{
  if (!self->watches_running)
    {
      poll_events_start_watches(self->poll_events);
      self->watches_running = TRUE;
    }
}

static gboolean
log_reader_is_opened(LogReader *self)
{
  return self->proto && self->poll_events;
}

static void
log_reader_suspend_until_awoken(LogReader *self)
{
  self->immediate_check = FALSE;
  poll_events_suspend_watches(self->poll_events);
  self->suspended = TRUE;
}

static void
log_reader_force_check_in_next_poll(LogReader *self)
{
  self->immediate_check = FALSE;
  poll_events_suspend_watches(self->poll_events);
  self->suspended = FALSE;

  if (!iv_task_registered(&self->restart_task))
    {
      iv_task_register(&self->restart_task);
    }
}

static void
log_reader_update_watches(LogReader *self)
{
  GIOCondition cond;
  gint idle_timeout = -1;

  main_loop_assert_main_thread();

  log_reader_stop_idle_timer(self);

  if (!log_reader_is_opened(self))
    return;

  log_reader_start_watches_if_stopped(self);

  gboolean free_to_send = log_source_free_to_send(&self->super);
  if (!free_to_send)
    {
      log_reader_suspend_until_awoken(self);
      return;
    }

  LogProtoPrepareAction prepare_action = log_proto_server_prepare(self->proto, &cond, &idle_timeout);

  if (idle_timeout > 0)
    {
      iv_validate_now();

      self->idle_timer.expires = iv_now;
      self->idle_timer.expires.tv_sec += idle_timeout;

      iv_timer_register(&self->idle_timer);
    }

  if (self->immediate_check)
    {
      log_reader_force_check_in_next_poll(self);
      return;
    }

  switch (prepare_action)
    {
    case LPPA_POLL_IO:
      poll_events_update_watches(self->poll_events, cond);
      break;
    case LPPA_FORCE_SCHEDULE_FETCH:
      log_reader_force_check_in_next_poll(self);
      break;
    case LPPA_SUSPEND:
      log_reader_suspend_until_awoken(self);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

static void
_add_aux_nvpair(const gchar *name, const gchar *value, gsize value_len, gpointer user_data)
{
  LogMessage *msg = (LogMessage *) user_data;

  log_msg_set_value_by_name(msg, name, value, value_len);;
}

static inline gint
log_reader_process_handshake(LogReader *self)
{
  LogProtoStatus status = log_proto_server_handshake(self->proto);

  switch (status)
    {
    case LPS_EOF:
    case LPS_ERROR:
      return status == LPS_ERROR ? NC_READ_ERROR : NC_CLOSE;
    case LPS_SUCCESS:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  return 0;
}

static gboolean
log_reader_handle_line(LogReader *self, const guchar *line, gint length, LogTransportAuxData *aux)
{
  LogMessage *m;

  msg_debug("Incoming log entry",
            evt_tag_printf("line", "%.*s", length, line));
  /* use the current time to get the time zone offset */
  m = log_msg_new((gchar *) line, length,
                  &self->options->parse_options);

  log_msg_set_saddr(m, aux->peer_addr ? : self->peer_addr);
  log_msg_refcache_start_producer(m);

  log_transport_aux_data_foreach(aux, _add_aux_nvpair, m);

  log_source_post(&self->super, m);
  log_msg_refcache_stop();
  return log_source_free_to_send(&self->super);
}

/* returns: notify_code (NC_XXXX) or 0 for success */
static gint
log_reader_fetch_log(LogReader *self)
{
  gint msg_count = 0;
  gboolean may_read = TRUE;
  LogTransportAuxData aux;

  log_transport_aux_data_init(&aux);
  if (log_proto_server_handshake_in_progress(self->proto))
    {
      return log_reader_process_handshake(self);
    }

  /* NOTE: this loop is here to decrease the load on the main loop, we try
   * to fetch a couple of messages in a single run (but only up to
   * fetch_limit).
   */
  while (msg_count < self->options->fetch_limit && !main_loop_worker_job_quit())
    {
      Bookmark *bookmark;
      const guchar *msg;
      gsize msg_len;
      LogProtoStatus status;

      msg = NULL;

      /* NOTE: may_read is used to implement multi-read checking. It
       * is initialized to TRUE to indicate that the protocol is
       * allowed to issue a read(). If multi-read is disallowed in the
       * protocol, it resets may_read to FALSE after the first read was issued.
       */

      log_transport_aux_data_reinit(&aux);
      bookmark = ack_tracker_request_bookmark(self->super.ack_tracker);
      status = log_proto_server_fetch(self->proto, &msg, &msg_len, &may_read, &aux, bookmark);
      switch (status)
        {
        case LPS_EOF:
          g_sockaddr_unref(aux.peer_addr);
          return NC_CLOSE;
        case LPS_ERROR:
          g_sockaddr_unref(aux.peer_addr);
          return NC_READ_ERROR;
        case LPS_SUCCESS:
          break;
        default:
          g_assert_not_reached();
          break;
        }

      if (!msg)
        {
          /* no more messages for now */
          break;
        }
      if (msg_len > 0 || (self->options->flags & LR_EMPTY_LINES))
        {
          msg_count++;

          ScratchBuffersMarker mark;
          scratch_buffers_mark(&mark);
          if (!log_reader_handle_line(self, msg, msg_len, &aux))
            {
              scratch_buffers_reclaim_marked(mark);
              /* window is full, don't generate further messages */
              break;
            }
          scratch_buffers_reclaim_marked(mark);
        }
    }
  log_transport_aux_data_destroy(&aux);

  if (msg_count == self->options->fetch_limit)
    self->immediate_check = TRUE;
  return 0;
}

static gboolean
log_reader_init(LogPipe *s)
{
  LogReader *self = (LogReader *) s;

  if (!log_source_init(s))
    return FALSE;

  if (!log_proto_server_validate_options(self->proto))
    return FALSE;

  if (!self->options->parse_options.format_handler)
    {
      msg_error("Unknown format plugin specified",
                evt_tag_str("format", self->options->parse_options.format));
      return FALSE;
    }

  poll_events_set_callback(self->poll_events, log_reader_io_process_input, self);

  log_reader_update_watches(self);
  iv_event_register(&self->schedule_wakeup);
  iv_event_register(&self->last_msg_sent_event);

  return TRUE;
}

static gboolean
log_reader_deinit(LogPipe *s)
{
  LogReader *self = (LogReader *) s;

  main_loop_assert_main_thread();

  iv_event_unregister(&self->schedule_wakeup);
  iv_event_unregister(&self->last_msg_sent_event);
  log_reader_stop_watches(self);
  log_reader_stop_idle_timer(self);

  if (!log_source_deinit(s))
    return FALSE;

  return TRUE;
}

static void
log_reader_free(LogPipe *s)
{
  LogReader *self = (LogReader *) s;

  if (self->proto)
    {
      log_proto_server_free(self->proto);
      self->proto = NULL;
    }
  if (self->poll_events)
    poll_events_free(self->poll_events);

  log_pipe_unref(self->control);
  g_sockaddr_unref(self->peer_addr);
  g_static_mutex_free(&self->pending_proto_lock);
  g_cond_free(self->pending_proto_cond);
  log_source_free(s);
}

void
log_reader_set_options(LogReader *s, LogPipe *control, LogReaderOptions *options,
                       const gchar *stats_id, const gchar *stats_instance)
{
  LogReader *self = (LogReader *) s;

  /* log_reader_reopen() needs to be called prior to set_options.  This is
   * an ugly hack, but at least it is more explicitly than what used to be
   * here, which silently ignored if self->proto was NULL.
   */

  g_assert(self->proto != NULL);
  gboolean pos_tracked = log_proto_server_is_position_tracked(self->proto);

  log_source_set_options(&self->super, &options->super, stats_id, stats_instance,
                         (options->flags & LR_THREADED), pos_tracked, control->expr_node);

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;

  self->options = options;
  log_proto_server_set_options(self->proto, &self->options->proto_options.super);
  log_proto_server_set_ack_tracker(self->proto, self->super.ack_tracker);
}

/* run in the main thread in reaction to a log_reader_reopen to change
 * the source LogProtoServer instance. It needs to be ran in the main
 * thread as it reregisters the watches associated with the main
 * thread. */
void
log_reader_reopen_deferred(gpointer s)
{
  gpointer *args = (gpointer *) s;
  LogReader *self = args[0];
  LogProtoServer *proto = args[1];
  PollEvents *poll_events = args[2];

  if (self->io_job.working)
    {
      self->pending_proto = proto;
      self->pending_poll_events = poll_events;
      self->pending_proto_present = TRUE;
      return;
    }

  log_reader_stop_watches(self);
  log_reader_stop_idle_timer(self);
  log_reader_apply_proto_and_poll_events(self, proto, poll_events);
  log_reader_update_watches(self);
}

void
log_reader_reopen(LogReader *self, LogProtoServer *proto, PollEvents *poll_events)
{
  gpointer args[] = { self, proto, poll_events };

  main_loop_call((MainLoopTaskFunc) log_reader_reopen_deferred, args, TRUE);

  if (!main_loop_is_main_thread())
    {
      g_static_mutex_lock(&self->pending_proto_lock);
      while (self->pending_proto_present)
        {
          g_cond_wait(self->pending_proto_cond, g_static_mutex_get_mutex(&self->pending_proto_lock));
        }
      g_static_mutex_unlock(&self->pending_proto_lock);
    }
}

void
log_reader_close_proto(LogReader *self)
{
  log_reader_reopen(self, NULL, NULL);
}

static void
log_reader_idle_timeout(void *cookie)
{
  LogReader *self = (LogReader *) cookie;

  msg_notice("Source timeout has elapsed, closing connection",
             evt_tag_int("fd", log_proto_server_get_fd(self->proto)));

  log_pipe_notify(self->control, NC_CLOSE, self);
}

void
log_reader_set_peer_addr(LogReader *s, GSockAddr *peer_addr)
{
  LogReader *self = (LogReader *) s;

  g_sockaddr_unref(self->peer_addr);
  self->peer_addr = g_sockaddr_ref(peer_addr);
}

LogReader *
log_reader_new(GlobalConfig *cfg)
{
  LogReader *self = g_new0(LogReader, 1);

  log_source_init_instance(&self->super, cfg);
  self->super.super.init = log_reader_init;
  self->super.super.deinit = log_reader_deinit;
  self->super.super.free_fn = log_reader_free;
  self->super.wakeup = log_reader_wakeup;
  self->super.window_empty_cb = log_reader_window_empty;
  self->immediate_check = FALSE;
  log_reader_init_watches(self);
  g_static_mutex_init(&self->pending_proto_lock);
  self->pending_proto_cond = g_cond_new();
  return self;
}

void
log_reader_set_immediate_check(LogReader *s)
{
  LogReader *self = (LogReader *) s;

  self->immediate_check = TRUE;
}

void
log_reader_options_defaults(LogReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  log_proto_server_options_defaults(&options->proto_options.super);
  msg_format_options_defaults(&options->parse_options);
  options->fetch_limit = 10;
}

/*
 * NOTE: _init needs to be idempotent when called multiple times w/o invoking _destroy
 *
 * Rationale:
 *   - init is called from driver init (e.g. affile_sd_init),
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_sd_deinit)
 *
 * The reason:
 *   - when initializing the reloaded configuration fails for some reason,
 *     we have to fall back to the old configuration, thus we cannot dump
 *     the information stored in the Options structure at deinit time, but
 *     have to recover it when the old configuration is initialized.
 *
 * For the reasons above, init and destroy behave the following way:
 *
 *   - init is idempotent, it can be called multiple times without leaking
 *     memory, and without loss of information
 *   - destroy is only called once, when the options are indeed to be destroyed
 *
 * Also important to note is that when init is called multiple times, the
 * GlobalConfig reference is the same, this means that it is enough to
 * remember whether init was called already and return w/o doing anything in
 * that case, which is actually how idempotency is implemented here.
 */
void
log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  if (options->initialized)
    return;

  log_source_options_init(&options->super, cfg, group_name);
  log_proto_server_options_init(&options->proto_options.super, cfg);
  msg_format_options_init(&options->parse_options, cfg);

  if (options->check_hostname == -1)
    options->check_hostname = cfg->check_hostname;
  if (options->check_hostname)
    {
      options->parse_options.flags |= LP_CHECK_HOSTNAME;
    }
  if (!options->super.keep_timestamp)
    {
      options->parse_options.flags |= LP_NO_PARSE_DATE;
    }
  if (options->parse_options.default_pri == 0xFFFF)
    {
      if (options->flags & LR_KERNEL)
        options->parse_options.default_pri = LOG_KERN | LOG_NOTICE;
      else
        options->parse_options.default_pri = LOG_USER | LOG_NOTICE;
    }
  if (options->proto_options.super.encoding)
    options->parse_options.flags |= LP_ASSUME_UTF8;
  if (cfg->threaded)
    options->flags |= LR_THREADED;
  options->initialized = TRUE;
}

void
log_reader_options_destroy(LogReaderOptions *options)
{
  log_source_options_destroy(&options->super);
  log_proto_server_options_destroy(&options->proto_options.super);
  msg_format_options_destroy(&options->parse_options);
  options->initialized = FALSE;
}

CfgFlagHandler log_reader_flag_handlers[] =
{
  /* NOTE: underscores are automatically converted to dashes */

  /* LogReaderOptions */
  { "kernel",                     CFH_SET, offsetof(LogReaderOptions, flags),               LR_KERNEL },
  { "empty-lines",                CFH_SET, offsetof(LogReaderOptions, flags),               LR_EMPTY_LINES },
  { "threaded",                   CFH_SET, offsetof(LogReaderOptions, flags),               LR_THREADED },
  { NULL },
};

gboolean
log_reader_options_process_flag(LogReaderOptions *options, const gchar *flag)
{
  if (!msg_format_options_process_flag(&options->parse_options, flag))
    return cfg_process_flag(log_reader_flag_handlers, options, flag);
  return TRUE;
}
