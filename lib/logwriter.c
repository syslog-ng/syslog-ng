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

#include "logwriter.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "hostname.h"
#include "host-resolve.h"
#include "seqnum.h"
#include "str-utils.h"
#include "find-crlf.h"
#include "mainloop.h"
#include "mainloop-io-worker.h"
#include "mainloop-call.h"
#include "ml-batched-timer.h"
#include "str-format.h"
#include "scratch-buffers.h"
#include "timeutils/format.h"
#include "timeutils/misc.h"

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iv.h>
#include <iv_event.h>
#include <iv_work.h>

typedef enum
{
  /* flush modes */

  /* business as usual, flush when the buffer is full */
  LW_FLUSH_NORMAL,
  /* flush the buffer immediately please */
  LW_FLUSH_FORCE,
} LogWriterFlushMode;

struct _LogWriter
{
  LogPipe super;
  LogQueue *queue;
  guint32 flags:31;
  gint32 seq_num;
  gboolean partial_write;
  StatsCounterItem *dropped_messages;
  StatsCounterItem *suppressed_messages;
  StatsCounterItem *processed_messages;
  StatsCounterItem *written_messages;
  LogPipe *control;
  LogWriterOptions *options;
  LogMessage *last_msg;
  guint32 last_msg_count;
  GString *line_buffer;

  gchar *stats_id;
  gchar *stats_instance;

  struct iv_fd fd_watch;
  struct iv_timer suspend_timer;
  struct iv_task immed_io_task;
  struct iv_event queue_filled;
  MainLoopIOWorkerJob io_job;
  GStaticMutex suppress_lock;
  MlBatchedTimer suppress_timer;
  MlBatchedTimer mark_timer;
  struct iv_timer reopen_timer;
  struct iv_timer idle_timer;
  gboolean work_result;
  gint pollable_state;
  LogProtoClient *proto, *pending_proto;
  gboolean watches_running:1, suspended:1, waiting_for_throttle:1;
  gboolean pending_proto_present;
  GCond *pending_proto_cond;
  GStaticMutex pending_proto_lock;
};

/**
 * LogWriter behaviour
 * ~~~~~~~~~~~~~~~~~~~
 *
 * LogWriter is a core element of syslog-ng sending messages out to some
 * kind of destination represented by a UNIX fd. Outgoing messages are sent
 * to the target asynchronously, first by placing them to a queue and then
 * sending messages when poll() indicates that the fd is writable.
 *
 *
 * Flow control
 * ------------
 * For a simple log writer without a disk buffer messages are placed on a
 * GQueue and they are acknowledged when the send() system call returned
 * success. This is more complex when disk buffering is used, in which case
 * messages are put to the "disk buffer" first and acknowledged immediately.
 * (this way the reader never stops when the disk buffer area is not yet
 * full). When disk buffer reaches its limit, messages are added to the the
 * usual GQueue and messages get acknowledged when they are moved to the
 * disk buffer.
 *
 **/

static gboolean log_writer_process_out(LogWriter *self);
static gboolean log_writer_process_in(LogWriter *self);
static void log_writer_broken(LogWriter *self, gint notify_code);
static void log_writer_start_watches(LogWriter *self);
static void log_writer_stop_watches(LogWriter *self);
static void log_writer_stop_idle_timer(LogWriter *self);
static void log_writer_update_watches(LogWriter *self);
static void log_writer_suspend(LogWriter *self);
static void log_writer_free_proto(LogWriter *self);
static void log_writer_set_proto(LogWriter *self, LogProtoClient *proto);
static void log_writer_set_pending_proto(LogWriter *self, LogProtoClient *proto, gboolean present);


static void
log_writer_msg_ack(gint num_msg_acked, gpointer user_data)
{
  LogWriter *self = (LogWriter *)user_data;
  log_queue_ack_backlog(self->queue, num_msg_acked);
}

void
log_writer_msg_rewind(LogWriter *self)
{
  log_queue_rewind_backlog_all(self->queue);
}

static void
log_writer_msg_rewind_cb(gpointer user_data)
{
  LogWriter *self = (LogWriter *)user_data;
  log_writer_msg_rewind(self);
}

void
log_writer_set_flags(LogWriter *self, guint32 flags)
{
  g_assert((self->super.flags & PIF_INITIALIZED) == 0);
  self->flags = flags;
}

guint32
log_writer_get_flags(LogWriter *self)
{
  return self->flags;
}

/* returns a reference */
LogQueue *
log_writer_get_queue(LogWriter *s)
{
  LogWriter *self = (LogWriter *) s;

  return log_queue_ref(self->queue);
}

/* consumes the reference */
void
log_writer_set_queue(LogWriter *self, LogQueue *queue)
{
  log_queue_unref(self->queue);
  self->queue = log_queue_ref(queue);
  log_queue_set_use_backlog(self->queue, TRUE);
}

static void
log_writer_work_perform(gpointer s, GIOCondition cond)
{
  LogWriter *self = (LogWriter *) s;

  g_assert((self->super.flags & PIF_INITIALIZED) != 0);
  g_assert((cond == G_IO_OUT) || (cond == G_IO_IN));

  if (cond == G_IO_OUT)
    self->work_result = log_writer_process_out(self);
  else if (cond == G_IO_IN)
    self->work_result = log_writer_process_in(self);
}

static void
log_writer_work_finished(gpointer s)
{
  LogWriter *self = (LogWriter *) s;

  main_loop_assert_main_thread();
  self->waiting_for_throttle = FALSE;

  if (self->pending_proto_present)
    {
      /* pending proto is only set in the main thread, so no need to
       * lock it before coming here. After we're syncing with the
       * log_writer_reopen() call, quite possibly coming from a
       * non-main thread. */

      g_static_mutex_lock(&self->pending_proto_lock);
      log_writer_free_proto(self);

      log_writer_set_proto(self, self->pending_proto);
      log_writer_set_pending_proto(self, NULL, FALSE);

      g_cond_signal(self->pending_proto_cond);
      g_static_mutex_unlock(&self->pending_proto_lock);
    }

  if (!self->work_result)
    {
      log_writer_broken(self, NC_WRITE_ERROR);
      if (self->proto)
        {
          log_writer_suspend(self);
          msg_notice("Suspending write operation because of an I/O error",
                     evt_tag_int("fd", log_proto_client_get_fd(self->proto)),
                     evt_tag_int("time_reopen", self->options->time_reopen));
        }
      return;
    }

  if ((self->super.flags & PIF_INITIALIZED) && self->proto)
    {
      /* reenable polling the source, but only if we're still initialized */
      log_writer_start_watches(self);
    }
}

static void
log_writer_io_handler(gpointer s, GIOCondition cond)
{
  LogWriter *self = (LogWriter *) s;

  main_loop_assert_main_thread();

  log_writer_stop_watches(self);
  log_writer_stop_idle_timer(self);
  if ((self->options->options & LWO_THREADED))
    {
      main_loop_io_worker_job_submit(&self->io_job, cond);
    }
  else
    {
      /* Checking main_loop_io_worker_job_quit() helps to speed up the
       * reload process.  If reload/shutdown is requested we shouldn't do
       * anything here, a final flush will be attempted in
       * log_writer_deinit().
       *
       * Our current understanding is that it doesn't prevent race
       * conditions of any kind.
       */

      if (!main_loop_worker_job_quit())
        {
          log_writer_work_perform(s, cond);
          log_writer_work_finished(s);
        }
    }
}

static void
log_writer_io_handle_out(gpointer s)
{
  log_writer_io_handler(s, G_IO_OUT);
}

static void
log_writer_io_handle_in(gpointer s)
{
  log_writer_io_handler(s, G_IO_IN);
}

static void
log_writer_io_error(gpointer s)
{
  LogWriter *self = (LogWriter *) s;

  if (self->fd_watch.handler_out == NULL && self->fd_watch.handler_in == NULL)
    {
      msg_debug("POLLERR occurred while idle",
                evt_tag_int("fd", log_proto_client_get_fd(self->proto)));
      log_writer_broken(self, NC_WRITE_ERROR);
      return;
    }
  else
    {
      /* in case we have an error state but we also asked for read/write
       * polling, the error should be handled by the I/O callback.  But we
       * need not call that explicitly as ivykis does that for us.  */
    }
  log_writer_update_watches(self);
}

static void
log_writer_io_check_eof(gpointer s)
{
  LogWriter *self = (LogWriter *) s;

  msg_error("EOF occurred while idle",
            evt_tag_int("fd", log_proto_client_get_fd(self->proto)));
  log_writer_broken(self, NC_CLOSE);
}

static void
log_writer_error_suspend_elapsed(gpointer s)
{
  LogWriter *self = (LogWriter *) s;

  self->suspended = FALSE;
  msg_notice("Error suspend timeout has elapsed, attempting to write again",
             evt_tag_int("fd", log_proto_client_get_fd(self->proto)));
  log_writer_start_watches(self);
}

static void
log_writer_update_fd_callbacks(LogWriter *self, GIOCondition cond)
{
  main_loop_assert_main_thread();
  if (self->pollable_state > 0)
    {
      if (cond & G_IO_IN)
        iv_fd_set_handler_in(&self->fd_watch, log_writer_io_handle_in);
      else if (self->flags & LW_DETECT_EOF)
        iv_fd_set_handler_in(&self->fd_watch, log_writer_io_check_eof);
      else
        iv_fd_set_handler_in(&self->fd_watch, NULL);

      if (cond & G_IO_OUT)
        iv_fd_set_handler_out(&self->fd_watch, log_writer_io_handle_out);
      else
        iv_fd_set_handler_out(&self->fd_watch, NULL);

      iv_fd_set_handler_err(&self->fd_watch, log_writer_io_error);
    }
  else
    {
      /* fd is not pollable, assume it is always writable */
      if (cond & G_IO_OUT)
        {
          if (!iv_task_registered(&self->immed_io_task))
            iv_task_register(&self->immed_io_task);
        }
      else if (iv_task_registered(&self->immed_io_task))
        {
          iv_task_unregister(&self->immed_io_task);
        }
    }
}

static void
log_writer_arm_suspend_timer(LogWriter *self, void (*handler)(void *), glong timeout_msec)
{
  main_loop_assert_main_thread();

  if (iv_timer_registered(&self->suspend_timer))
    iv_timer_unregister(&self->suspend_timer);
  iv_validate_now();
  self->suspend_timer.handler = handler;
  self->suspend_timer.expires = iv_now;
  timespec_add_msec(&self->suspend_timer.expires, timeout_msec);
  iv_timer_register(&self->suspend_timer);
}

static void
log_writer_queue_filled(gpointer s)
{
  LogWriter *self = (LogWriter *) s;

  main_loop_assert_main_thread();

  /*
   * NOTE: This theory is somewhat questionable, e.g. I'm not 100% sure it
   * is the right scenario, but the race was closed.  So take this with a
   * grain of salt.
   *
   * The queue_filled callback is running in the main thread. Because of the
   * possible delay caused by iv_event_post() the callback might be
   * delivered event after stop_watches() has been called.
   *
   *   - log_writer_schedule_update_watches() is called by the reader
   *     thread, which calls iv_event_post()
   *   - the main thread calls stop_watches() in work_perform
   *   - the event is delivered in the main thread
   *
   * But since stop/start watches always run in the main thread and we do
   * too, we can check if this is the case.  A LogWriter without watches
   * running is busy writing out data to the destination, e.g.  a
   * start_watches is to be expected once log_writer_work_finished() is run
   * at the end of the deferred work, executed by the I/O threads.
   */
  if (self->watches_running)
    log_writer_update_watches((LogWriter *) s);
}

/* NOTE: runs in the source thread */
static void
log_writer_schedule_update_watches(LogWriter *self)
{
  iv_event_post(&self->queue_filled);
}

static void
log_writer_suspend(LogWriter *self)
{
  /* flush code indicates that we need to suspend our writing activities
   * until time_reopen elapses */

  log_writer_arm_suspend_timer(self, log_writer_error_suspend_elapsed, self->options->time_reopen * 1000L);
  self->suspended = TRUE;
}

static void
log_writer_update_watches(LogWriter *self)
{
  gint fd;
  GIOCondition cond = 0;
  gint timeout_msec = 0;
  gint idle_timeout = -1;

  main_loop_assert_main_thread();

  log_writer_stop_idle_timer(self);

  /* NOTE: we either start the suspend_timer or enable the fd_watch. The two MUST not happen at the same time. */

  if (log_proto_client_prepare(self->proto, &fd, &cond, &idle_timeout) ||
      self->waiting_for_throttle ||
      log_queue_check_items(self->queue, &timeout_msec,
                            (LogQueuePushNotifyFunc) log_writer_schedule_update_watches, self, NULL))
    {
      /* flush_lines number of element is already available and throttle would permit us to send. */
      log_writer_update_fd_callbacks(self, cond);
    }
  else if (timeout_msec)
    {
      /* few elements are available, but less than flush_lines, we need to start a timer to initiate a flush */

      log_writer_update_fd_callbacks(self, 0);
      self->waiting_for_throttle = TRUE;
      log_writer_arm_suspend_timer(self, (void (*)(void *)) log_writer_update_watches, (glong)timeout_msec);
    }
  else
    {
      /* no elements or no throttle space, wait for a wakeup by the queue
       * when the required number of items are added.  see the
       * log_queue_check_items and its parallel_push argument above
       */
      log_writer_update_fd_callbacks(self, 0);
    }

  if (idle_timeout > 0)
    {
      iv_validate_now();

      self->idle_timer.expires = iv_now;
      self->idle_timer.expires.tv_sec += idle_timeout;

      iv_timer_register(&self->idle_timer);
    }
}

static gboolean
is_file_regular(gint fd)
{
  struct stat st;

  if (fstat(fd, &st) >= 0)
    {
      return S_ISREG(st.st_mode);
    }

  /* if stat fails, that's interesting, but we should probably poll
   * it, hopefully that's less likely to cause spinning */

  return FALSE;
}

static void
log_writer_start_watches(LogWriter *self)
{
  gint fd;
  GIOCondition cond;
  gint idle_timeout = -1;

  if (self->watches_running)
    return;

  log_proto_client_prepare(self->proto, &fd, &cond, &idle_timeout);

  self->fd_watch.fd = fd;

  if (self->pollable_state < 0)
    {
      if (is_file_regular(fd))
        self->pollable_state = 0;
      else
        self->pollable_state = !iv_fd_register_try(&self->fd_watch);
    }
  else if (self->pollable_state > 0)
    iv_fd_register(&self->fd_watch);

  log_writer_update_watches(self);
  self->watches_running = TRUE;
}

static void
log_writer_stop_watches(LogWriter *self)
{
  if (self->watches_running)
    {
      if (iv_timer_registered(&self->reopen_timer))
        iv_timer_unregister(&self->reopen_timer);
      if (iv_timer_registered(&self->suspend_timer))
        iv_timer_unregister(&self->suspend_timer);
      if (iv_fd_registered(&self->fd_watch))
        iv_fd_unregister(&self->fd_watch);
      if (iv_task_registered(&self->immed_io_task))
        iv_task_unregister(&self->immed_io_task);

      log_queue_reset_parallel_push(self->queue);

      self->watches_running = FALSE;
    }
}

static void
log_writer_stop_idle_timer(LogWriter *self)
{
  if (iv_timer_registered(&self->idle_timer))
    iv_timer_unregister(&self->idle_timer);
}

static void
log_writer_arm_suppress_timer(LogWriter *self)
{
  ml_batched_timer_postpone(&self->suppress_timer, self->options->suppress);
}

/**
 * Remember the last message for dup detection.
 *
 * NOTE: suppress_lock must be held.
 **/
static void
log_writer_record_last_message(LogWriter *self, LogMessage *lm)
{
  if (self->last_msg)
    log_msg_unref(self->last_msg);

  log_msg_ref(lm);
  self->last_msg = lm;
  self->last_msg_count = 0;
}

/*
 * NOTE: suppress_lock must be held.
 */
static void
log_writer_release_last_message(LogWriter *self)
{
  if (self->last_msg)
    log_msg_unref(self->last_msg);

  self->last_msg = NULL;
  self->last_msg_count = 0;
}

/*
 * NOTE: suppress_lock must be held.
 */
static void
log_writer_emit_suppress_summary(LogWriter *self)
{
  LogMessage *m;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gchar buf[1024];
  gssize len;
  const gchar *p;

  msg_debug("Suppress timer elapsed, emitting suppression summary");

  len = g_snprintf(buf, sizeof(buf), "Last message '%.20s' repeated %d times, suppressed by syslog-ng on %s",
                   log_msg_get_value(self->last_msg, LM_V_MESSAGE, NULL),
                   self->last_msg_count,
                   get_local_hostname_fqdn());

  m = log_msg_new_internal(self->last_msg->pri, buf);

  p = log_msg_get_value(self->last_msg, LM_V_HOST, &len);
  log_msg_set_value(m, LM_V_HOST, p, len);
  p = log_msg_get_value(self->last_msg, LM_V_PROGRAM, &len);
  log_msg_set_value(m, LM_V_PROGRAM, p, len);

  path_options.ack_needed = FALSE;

  log_queue_push_tail(self->queue, m, &path_options);
  log_writer_release_last_message(self);
}

static gboolean
log_writer_suppress_timeout(gpointer pt)
{
  LogWriter *self = (LogWriter *) pt;

  main_loop_assert_main_thread();

  /* NOTE: this will probably do nothing as we are the timer callback, but
   * we may not do it with the suppress_lock held */
  ml_batched_timer_cancel(&self->suppress_timer);
  g_static_mutex_lock(&self->suppress_lock);

  /* NOTE: we may be waken up an extra time if the suppress_timer setup race
   * is lost, see the comment at log_writer_is_msg_suppressed() for an
   * explanation */
  if (self->last_msg_count > 0)
    log_writer_emit_suppress_summary(self);
  g_static_mutex_unlock(&self->suppress_lock);

  return FALSE;
}

static gboolean
_is_message_a_mark(LogMessage *msg)
{
  gssize msg_len;
  const gchar *value = log_msg_get_value(msg, LM_V_MESSAGE, &msg_len);
  return strncmp(value, "-- MARK --", msg_len) == 0;
}

static gboolean
_is_message_a_repetition(LogMessage *msg, LogMessage *last)
{
  return strcmp(log_msg_get_value(last, LM_V_MESSAGE, NULL), log_msg_get_value(msg, LM_V_MESSAGE, NULL)) == 0 &&
         strcmp(log_msg_get_value(last, LM_V_HOST, NULL), log_msg_get_value(msg, LM_V_HOST, NULL)) == 0 &&
         strcmp(log_msg_get_value(last, LM_V_PROGRAM, NULL), log_msg_get_value(msg, LM_V_PROGRAM, NULL)) == 0 &&
         strcmp(log_msg_get_value(last, LM_V_PID, NULL), log_msg_get_value(msg, LM_V_PID, NULL)) == 0;
}

static gboolean
_is_time_within_the_suppress_timeout(LogWriter *self, LogMessage *msg)
{
  return self->last_msg->timestamps[LM_TS_RECVD].ut_sec >= msg->timestamps[LM_TS_RECVD].ut_sec - self->options->suppress;
}

/**
 * log_writer_is_msg_suppressed:
 *
 * This function is called to suppress duplicate messages from a given host.
 *
 * Locking notes:
 *
 * There's a strict ordering requirement between suppress_lock and
 * interacting with the main loop (which ml_batched_timer beind
 * suppress_timer is doing).
 *
 * The reason is that the main thread (running
 * the main loop) sometimes acquires suppress_lock (at suppress timer
 * expiration) and while blocking on suppress_lock it cannot service
 * main_loop_calls()
 *
 * This function makes it sure that ml_batched_timer_update/cancel calls are
 * only done with the suppress lock released.
 *
 * If we do this, we might have a few unfortunate side effects due to races
 * that we also try to handle:
 *
 * Two messages race, one of these matches the recorded last message,
 * the other doesn't. In this case, moving the update on the suppress_timer
 * outside of the lock region might cause two different races:
 *
 * 1) matching message comes first, then non-matching
 *
 * This case the suppress_lock protected region decides that the suppress
 * timer needs to fire (#1) and then the other decides that it needs to
 * be cancelled. (#2)
 *
 * If these are processed in order, then we are the same as if the two was
 * also protected by the mutex (which is ok)
 *
 * If they are reversed, e.g. we first cancels the timer and the second arms it,
 * then we might have a timer wakeup which will find no suppressed messages
 * to report (as the non-matching message will set last_msg_count to zero). This
 * spurious wakeup should be handled by the expiration callback.
 *
 * 1) non-matching message comes first, then matching
 *
 * This is simply a message reordering case, e.g. we don't
 * want any suppressions to be emitted.
 *
 * In this case the locked regions finds that neither messages matched
 * the recorded one, thus both times they decide to cancel the timer, which
 * is ok. Timer cancellation can be reordered as they will have the same
 * effect anyway.
 *
 * Returns TRUE to indicate that the message is to be suppressed.
 **/
static gboolean
log_writer_is_msg_suppressed(LogWriter *self, LogMessage *lm)
{
  gboolean need_to_arm_suppress_timer;
  gboolean need_to_cancel_suppress_timer = FALSE;

  if (self->options->suppress <= 0)
    return FALSE;

  g_static_mutex_lock(&self->suppress_lock);
  if (self->last_msg)
    {
      if (_is_time_within_the_suppress_timeout(self, lm) &&
          _is_message_a_repetition(lm, self->last_msg) &&
          !_is_message_a_mark(lm))
        {

          stats_counter_inc(self->suppressed_messages);
          self->last_msg_count++;

          /* we only create the timer if this is the first suppressed message, otherwise it is already running. */
          need_to_arm_suppress_timer = self->last_msg_count == 1;
          g_static_mutex_unlock(&self->suppress_lock);

          /* this has to be outside of suppress_lock */
          if (need_to_arm_suppress_timer)
            log_writer_arm_suppress_timer(self);

          msg_debug("Suppressing duplicate message",
                    evt_tag_str("host", log_msg_get_value(lm, LM_V_HOST, NULL)),
                    evt_tag_str("msg", log_msg_get_value(lm, LM_V_MESSAGE, NULL)));
          return TRUE;
        }
      else
        {
          if (self->last_msg_count)
            log_writer_emit_suppress_summary(self);
          else
            log_writer_release_last_message(self);
          need_to_cancel_suppress_timer = TRUE;
        }
    }

  log_writer_record_last_message(self, lm);
  g_static_mutex_unlock(&self->suppress_lock);
  if (need_to_cancel_suppress_timer)
    ml_batched_timer_cancel(&self->suppress_timer);
  return FALSE;
}

static void
log_writer_postpone_mark_timer(LogWriter *self)
{
  if (self->options->mark_freq > 0)
    ml_batched_timer_postpone(&self->mark_timer, self->options->mark_freq);
}

/* this is the callback function that gets called when the MARK timeout
 * elapsed. It runs in the main thread.
 */
static void
log_writer_mark_timeout(void *cookie)
{
  LogWriter *self = (LogWriter *)cookie;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  const gchar *hostname;
  gsize hostname_len;
  LogMessage *msg;

  main_loop_assert_main_thread();

  msg = log_msg_new_mark();
  path_options.ack_needed = FALSE;
  /* timeout: there was no new message on the writer or it is in periodical mode */
  hostname = resolve_sockaddr_to_hostname(&hostname_len, msg->saddr, &self->options->host_resolve_options);

  log_msg_set_value(msg, LM_V_HOST, hostname, hostname_len);

  if (!log_writer_is_msg_suppressed(self, msg))
    {
      log_queue_push_tail(self->queue, msg, &path_options);
      stats_counter_inc(self->processed_messages);
    }
  else
    {
      log_msg_drop(msg, &path_options, AT_PROCESSED);
    }

  /* we need to issue another MARK in all mark-mode cases that already
   * triggered this callback (dst-idle, host-idle, periodical).  The
   * original setup of the timer is at a different location:
   *   - log_writer_queue() for "*-idle" modes
   *   - log_writer_init() for periodical mode
   */
  log_writer_postpone_mark_timer(self);
}

/* NOTE: runs in the reader thread */
static void
log_writer_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options)
{
  LogWriter *self = (LogWriter *) s;
  LogPathOptions local_options;
  gint mark_mode = self->options->mark_mode;

  if (!path_options->flow_control_requested &&
      ((self->proto == NULL || self->suspended) || !(self->flags & LW_SOFT_FLOW_CONTROL)))
    {
      /* NOTE: this code ACKs the message back if there's a write error in
       * order not to hang the client in case of a disk full */

      path_options = log_msg_break_ack(lm, path_options, &local_options);
    }

  if (log_writer_is_msg_suppressed(self, lm))
    {
      log_msg_drop(lm, path_options, AT_PROCESSED);
      return;
    }

  if (mark_mode != MM_INTERNAL && (lm->flags & LF_INTERNAL) && (lm->flags & LF_MARK))
    {
      /* drop MARK messages generated by internal() in case our mark-mode != internal */
      log_msg_drop(lm, path_options, AT_PROCESSED);
      return;
    }

  if (mark_mode == MM_DST_IDLE || (mark_mode == MM_HOST_IDLE && !(lm->flags & LF_LOCAL)))
    {
      /* in dst-idle and host-idle most, messages postpone the MARK itself */
      log_writer_postpone_mark_timer(self);
    }

  stats_counter_inc(self->processed_messages);
  log_queue_push_tail(self->queue, lm, path_options);
}

static void
log_writer_append_value(GString *result, LogMessage *lm, NVHandle handle, gboolean use_nil, gboolean append_space)
{
  const gchar *value;
  gssize value_len;

  value = log_msg_get_value(lm, handle, &value_len);
  if (use_nil && value_len == 0)
    g_string_append_c(result, '-');
  else
    {
      gchar *space;

      space = strchr(value, ' ');

      if (!space)
        g_string_append_len(result, value, value_len);
      else
        g_string_append_len(result, value, space - value);
    }
  if (append_space)
    g_string_append_c(result, ' ');
}

static void
log_writer_do_padding(LogWriter *self, GString *result)
{
  if (!self->options->padding)
    return;

  if(G_UNLIKELY(self->options->padding < result->len))
    {
      msg_warning("Padding is too small to hold the full message",
                  evt_tag_int("padding", self->options->padding),
                  evt_tag_int("msg_size", result->len));
      g_string_set_size(result, self->options->padding);
      return;
    }
  /* store the original length of the result */
  gint len = result->len;
  gint padd_bytes = self->options->padding - result->len;
  /* set the size to the padded size, this will allocate the string */
  g_string_set_size(result, self->options->padding);
  memset(result->str + len - 1, '\0', padd_bytes);
}

void
log_writer_format_log(LogWriter *self, LogMessage *lm, GString *result)
{
  LogTemplate *template = NULL;
  UnixTime *stamp;
  guint32 seq_num;
  static NVHandle meta_seqid = 0;

  if (!meta_seqid)
    meta_seqid = log_msg_get_value_handle(".SDATA.meta.sequenceId");

  if (lm->flags & LF_LOCAL)
    {
      seq_num = self->seq_num;
    }
  else
    {
      const gchar *seqid;
      gssize seqid_length;

      seqid = log_msg_get_value(lm, meta_seqid, &seqid_length);
      APPEND_ZERO(seqid, seqid, seqid_length);
      if (seqid[0])
        seq_num = strtol(seqid, NULL, 10);
      else
        seq_num = 0;
    }

  /* no template was specified, use default */
  stamp = &lm->timestamps[LM_TS_STAMP];

  g_string_truncate(result, 0);

  if ((self->flags & LW_SYSLOG_PROTOCOL) || (self->options->options & LWO_SYSLOG_PROTOCOL))
    {
      gssize len;

      /* we currently hard-wire version 1 */
      g_string_append_c(result, '<');
      format_uint32_padded(result, 0, 0, 10, lm->pri);
      g_string_append_c(result, '>');
      g_string_append_c(result, '1');
      g_string_append_c(result, ' ');

      append_format_unix_time(stamp, result, TS_FMT_ISO,
                              time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->ut_sec),
                              self->options->template_options.frac_digits);
      g_string_append_c(result, ' ');

      log_writer_append_value(result, lm, LM_V_HOST, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_PROGRAM, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_PID, TRUE, TRUE);
      log_writer_append_value(result, lm, LM_V_MSGID, TRUE, TRUE);

      len = result->len;
      log_msg_append_format_sdata(lm, result, seq_num);
      if (len == result->len)
        {
          /* NOTE: sd_param format did not generate any output, take it as an empty SD string */
          g_string_append_c(result, '-');
        }

      if (self->options->template)
        {
          g_string_append_c(result, ' ');
          if (lm->flags & LF_UTF8)
            g_string_append_len(result, "\xEF\xBB\xBF", 3);
          log_template_append_format(self->options->template, lm,
                                     &self->options->template_options,
                                     LTZ_SEND,
                                     seq_num, NULL,
                                     result);
        }
      else
        {
          const gchar *p;

          p = log_msg_get_value(lm, LM_V_MESSAGE, &len);
          g_string_append_c(result, ' ');
          if (len != 0)
            {
              if (lm->flags & LF_UTF8)
                g_string_append_len(result, "\xEF\xBB\xBF", 3);

              g_string_append_len(result, p, len);
            }
        }
      g_string_append_c(result, '\n');
      log_writer_do_padding(self, result);
    }
  else
    {

      if (self->options->template)
        {
          template = self->options->template;
        }
      else if (self->flags & LW_FORMAT_FILE)
        {
          template = self->options->file_template;
        }
      else if ((self->flags & LW_FORMAT_PROTO))
        {
          template = self->options->proto_template;
        }

      if (template)
        {
          log_template_format(template, lm,
                              &self->options->template_options,
                              LTZ_SEND,
                              seq_num, NULL,
                              result);

        }
      else
        {
          const gchar *p;
          gssize len;

          if (self->flags & LW_FORMAT_FILE)
            {
              format_unix_time(stamp, result, self->options->template_options.ts_format,
                               time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->ut_sec),
                               self->options->template_options.frac_digits);
            }
          else if (self->flags & LW_FORMAT_PROTO)
            {
              g_string_append_c(result, '<');
              format_uint32_padded(result, 0, 0, 10, lm->pri);
              g_string_append_c(result, '>');

              /* always use BSD timestamp by default, the use can override this using a custom template */
              append_format_unix_time(stamp, result, TS_FMT_BSD,
                                      time_zone_info_get_offset(self->options->template_options.time_zone_info[LTZ_SEND], stamp->ut_sec),
                                      self->options->template_options.frac_digits);
            }
          g_string_append_c(result, ' ');

          p = log_msg_get_value(lm, LM_V_HOST, &len);
          g_string_append_len(result, p, len);
          g_string_append_c(result, ' ');

          p = log_msg_get_value(lm, LM_V_LEGACY_MSGHDR, &len);
          if (len > 0)
            {
              g_string_append_len(result, p, len);
            }
          else
            {
              p = log_msg_get_value(lm, LM_V_PROGRAM, &len);
              if (len > 0)
                {
                  g_string_append_len(result, p, len);
                  p = log_msg_get_value(lm, LM_V_PID, &len);
                  if (len > 0)
                    {
                      g_string_append_c(result, '[');
                      g_string_append_len(result, p, len);
                      g_string_append_c(result, ']');
                    }
                  g_string_append_len(result, ": ", 2);
                }
            }
          p = log_msg_get_value(lm, LM_V_MESSAGE, &len);
          g_string_append_len(result, p, len);
          g_string_append_c(result, '\n');
          log_writer_do_padding(self, result);
        }
    }
  if (self->options->options & LWO_NO_MULTI_LINE)
    {
      gchar *p;

      p = result->str;
      /* NOTE: the size is calculated to leave trailing new line */
      while ((p = find_cr_or_lf(p, result->str + result->len - p - 1)))
        {
          *p = ' ';
          p++;
        }

    }
}

static void
log_writer_broken(LogWriter *self, gint notify_code)
{
  log_writer_stop_watches(self);
  log_writer_stop_idle_timer(self);
  log_pipe_notify(self->control, notify_code, self);
}

static void
log_writer_realloc_line_buffer(LogWriter *self)
{
  self->line_buffer->str = g_malloc(self->line_buffer->allocated_len);
  self->line_buffer->str[0] = 0;
  self->line_buffer->len = 0;
}

/*
 * Write messages to the underlying file descriptor using the installed
 * LogProtoClient instance.  This is called whenever the output is ready to accept
 * further messages, and once during config deinitialization, in order to
 * flush messages still in the queue, in the hope that most of them can be
 * written out.
 *
 * In threaded mode, this function is invoked as part of the "output" task
 * (in essence, this is the function that performs the output task).
 *
 */

static gboolean
log_writer_flush_finalize(LogWriter *self)
{
  LogProtoStatus status = log_proto_client_flush(self->proto);

  if (status == LPS_SUCCESS || status == LPS_PARTIAL)
    return TRUE;


  return FALSE;
}

gboolean
log_writer_write_message(LogWriter *self, LogMessage *msg, LogPathOptions *path_options, gboolean *write_error)
{
  gboolean consumed = FALSE;

  *write_error = FALSE;

  log_msg_refcache_start_consumer(msg, path_options);
  msg_set_context(msg);

  log_writer_format_log(self, msg, self->line_buffer);

  if (!(msg->flags & LF_INTERNAL))
    {
      msg_debug("Outgoing message",
                evt_tag_printf("message", "%s", self->line_buffer->str));
    }

  if (self->line_buffer->len)
    {
      LogProtoStatus status = log_proto_client_post(self->proto, msg, (guchar *)self->line_buffer->str,
                                                    self->line_buffer->len,
                                                    &consumed);

      self->partial_write = (status == LPS_PARTIAL);

      if (consumed)
        log_writer_realloc_line_buffer(self);

      if (status == LPS_ERROR)
        {
          if ((self->options->options & LWO_IGNORE_ERRORS) != 0)
            {
              if (!consumed)
                {
                  g_free(self->line_buffer->str);
                  log_writer_realloc_line_buffer(self);
                  consumed = TRUE;
                }
            }
          else
            {
              *write_error = TRUE;
              consumed = FALSE;
            }
        }
    }
  else
    {
      msg_debug("Error posting log message as template() output resulted in an empty string, skipping message");
      consumed = TRUE;
    }

  if (consumed)
    {
      if (msg->flags & LF_LOCAL)
        step_sequence_number(&self->seq_num);

      log_msg_unref(msg);
      msg_set_context(NULL);
      log_msg_refcache_stop();

      return TRUE;
    }
  else
    {
      msg_debug("Can't send the message rewind backlog",
                evt_tag_printf("message", "%s", self->line_buffer->str));

      log_queue_rewind_backlog(self->queue, 1);

      log_msg_unref(msg);
      msg_set_context(NULL);
      log_msg_refcache_stop();

      return FALSE;
    }
}

static inline LogMessage *
log_writer_queue_pop_message(LogWriter *self, LogPathOptions *path_options, gboolean force_flush)
{
  if (force_flush)
    return log_queue_pop_head_ignore_throttle(self->queue, path_options);
  else
    return log_queue_pop_head(self->queue, path_options);
}

static inline gboolean
log_writer_process_handshake(LogWriter *self)
{
  LogProtoStatus status = log_proto_client_handshake(self->proto);

  if (status != LPS_SUCCESS)
    return FALSE;

  return TRUE;
}

/*
 * @flush_mode specifies how hard LogWriter is trying to send messages to
 * the actual destination:
 *
 *
 * LW_FLUSH_NORMAL    - business as usual, flush when the buffer is full
 * LW_FLUSH_FORCE     - flush the buffer immediately please
 *
 */
gboolean
log_writer_flush(LogWriter *self, LogWriterFlushMode flush_mode)
{
  gboolean write_error = FALSE;

  if (!self->proto)
    return FALSE;

  if (log_proto_client_handshake_in_progress(self->proto))
    {
      return log_writer_process_handshake(self);
    }

  /* NOTE: in case we're reloading or exiting we flush all queued items as
   * long as the destination can consume it.  This is not going to be an
   * infinite loop, since the reader will cease to produce new messages when
   * main_loop_io_worker_job_quit() is set. */

  while ((!main_loop_worker_job_quit() || flush_mode == LW_FLUSH_FORCE) && !write_error)
    {
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      LogMessage *msg = log_writer_queue_pop_message(self, &path_options, flush_mode == LW_FLUSH_FORCE);

      if (!msg)
        break;

      ScratchBuffersMarker mark;
      scratch_buffers_mark(&mark);
      if (!log_writer_write_message(self, msg, &path_options, &write_error))
        {
          scratch_buffers_reclaim_marked(mark);
          break;
        }
      scratch_buffers_reclaim_marked(mark);

      if (!write_error)
        stats_counter_inc(self->written_messages);
    }

  if (write_error)
    return FALSE;

  return log_writer_flush_finalize(self);
}

gboolean
log_writer_forced_flush(LogWriter *self)
{
  return log_writer_flush(self, LW_FLUSH_FORCE);
}

gboolean
log_writer_process_in(LogWriter *self)
{
  if (!self->proto)
    return FALSE;

  return (log_proto_client_process_in(self->proto) == LPS_SUCCESS);
}

gboolean
log_writer_process_out(LogWriter *self)
{
  return log_writer_flush(self, LW_FLUSH_NORMAL);
}

static void
log_writer_reopen_timeout(void *cookie)
{
  LogWriter *self = (LogWriter *)cookie;

  log_pipe_notify(self->control, NC_REOPEN_REQUIRED, self);
}

static void
log_writer_idle_timeout(void *cookie)
{
  LogWriter *self = (LogWriter *) cookie;

  g_assert(!self->io_job.working);
  msg_notice("Destination timeout has elapsed, closing connection",
             evt_tag_int("fd", log_proto_client_get_fd(self->proto)));

  log_pipe_notify(self->control, NC_CLOSE, self);
}

static void
log_writer_init_watches(LogWriter *self)
{
  IV_FD_INIT(&self->fd_watch);
  self->fd_watch.cookie = self;

  IV_TASK_INIT(&self->immed_io_task);
  self->immed_io_task.cookie = self;
  self->immed_io_task.handler = log_writer_io_handle_out;

  IV_TIMER_INIT(&self->suspend_timer);
  self->suspend_timer.cookie = self;

  ml_batched_timer_init(&self->suppress_timer);
  self->suppress_timer.cookie = self;
  self->suppress_timer.handler = (void (*)(void *)) log_writer_suppress_timeout;
  self->suppress_timer.ref_cookie = (gpointer (*)(gpointer)) log_pipe_ref;
  self->suppress_timer.unref_cookie = (void (*)(gpointer)) log_pipe_unref;

  ml_batched_timer_init(&self->mark_timer);
  self->mark_timer.cookie = self;
  self->mark_timer.handler = log_writer_mark_timeout;
  self->mark_timer.ref_cookie = (gpointer (*)(gpointer)) log_pipe_ref;
  self->mark_timer.unref_cookie = (void (*)(gpointer)) log_pipe_unref;

  IV_TIMER_INIT(&self->reopen_timer);
  self->reopen_timer.cookie = self;
  self->reopen_timer.handler = log_writer_reopen_timeout;

  IV_TIMER_INIT(&self->idle_timer);
  self->idle_timer.cookie = self;
  self->idle_timer.handler = log_writer_idle_timeout;

  IV_EVENT_INIT(&self->queue_filled);
  self->queue_filled.cookie = self;
  self->queue_filled.handler = log_writer_queue_filled;

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = (void (*)(void *, GIOCondition)) log_writer_work_perform;
  self->io_job.completion = (void (*)(void *)) log_writer_work_finished;
  self->io_job.engage = (void (*)(void *)) log_pipe_ref;
  self->io_job.release = (void (*)(void *)) log_pipe_unref;
}

static void
_register_counters(LogWriter *self)
{
  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source, SCS_DESTINATION, self->stats_id,
                                  self->stats_instance);

    if (self->options->suppress > 0)
      stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_SUPPRESSED, &self->suppressed_messages);
    stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
    stats_register_counter(self->options->stats_level, &sc_key, SC_TYPE_WRITTEN, &self->written_messages);
    log_queue_register_stats_counters(self->queue, self->options->stats_level, &sc_key);
  }
  stats_unlock();
}

static gboolean
log_writer_init(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;

  if (self->queue == NULL)
    {
      return FALSE;
    }
  iv_event_register(&self->queue_filled);

  if ((self->options->options & LWO_NO_STATS) == 0 && !self->dropped_messages)
    _register_counters(self);

  if (self->proto)
    {
      LogProtoClient *proto;

      proto = self->proto;
      log_writer_set_proto(self, NULL);
      log_writer_reopen(self, proto);
    }

  if (self->options->mark_mode == MM_PERIODICAL)
    {
      /* periodical marks should be emitted even if no message is received,
       * so we need a timer right from the start */

      log_writer_postpone_mark_timer(self);
    }

  return TRUE;
}

static void
_unregister_counters(LogWriter *self)
{
  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, self->options->stats_source, SCS_DESTINATION, self->stats_id,
                                  self->stats_instance);

    stats_unregister_counter(&sc_key, SC_TYPE_DROPPED, &self->dropped_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_SUPPRESSED, &self->suppressed_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &self->processed_messages);
    stats_unregister_counter(&sc_key, SC_TYPE_WRITTEN, &self->written_messages);
    log_queue_unregister_stats_counters(self->queue, &sc_key);
  }
  stats_unlock();

}

static gboolean
log_writer_deinit(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;

  main_loop_assert_main_thread();

  log_queue_reset_parallel_push(self->queue);
  log_writer_forced_flush(self);
  /* FIXME: by the time we arrive here, it must be guaranteed that no
   * _queue() call is running in a different thread, otherwise we'd need
   * some kind of locking. */

  log_writer_stop_watches(self);
  log_writer_stop_idle_timer(self);

  iv_event_unregister(&self->queue_filled);

  if (iv_timer_registered(&self->reopen_timer))
    iv_timer_unregister(&self->reopen_timer);

  ml_batched_timer_unregister(&self->suppress_timer);
  ml_batched_timer_unregister(&self->mark_timer);

  _unregister_counters(self);

  return TRUE;
}

static void
log_writer_free(LogPipe *s)
{
  LogWriter *self = (LogWriter *) s;

  log_writer_free_proto(self);

  if (self->line_buffer)
    g_string_free(self->line_buffer, TRUE);

  log_queue_unref(self->queue);
  if (self->last_msg)
    log_msg_unref(self->last_msg);
  g_free(self->stats_id);
  g_free(self->stats_instance);
  ml_batched_timer_free(&self->mark_timer);
  ml_batched_timer_free(&self->suppress_timer);
  g_static_mutex_free(&self->suppress_lock);
  g_static_mutex_free(&self->pending_proto_lock);
  g_cond_free(self->pending_proto_cond);

  log_pipe_free_method(s);
}

/* FIXME: this is inherently racy */
gboolean
log_writer_has_pending_writes(LogWriter *self)
{
  return !log_queue_is_empty_racy(self->queue) || !self->watches_running;
}

gboolean
log_writer_opened(LogWriter *self)
{
  return self->proto != NULL;
}


static void
log_writer_free_proto(LogWriter *self)
{
  if (self->proto)
    log_proto_client_free(self->proto);

  self->proto = NULL;
}

static void
log_writer_set_proto(LogWriter *self, LogProtoClient *proto)
{
  self->proto = proto;

  if (proto)
    {
      LogProtoClientFlowControlFuncs flow_control_funcs;
      flow_control_funcs.ack_callback = log_writer_msg_ack;
      flow_control_funcs.rewind_callback = log_writer_msg_rewind_cb;
      flow_control_funcs.user_data = self;

      log_proto_client_set_client_flow_control(self->proto, &flow_control_funcs);
    }
}

static void
log_writer_set_pending_proto(LogWriter *self, LogProtoClient *proto, gboolean present)
{
  self->pending_proto = proto;
  self->pending_proto_present = present;
}

/* run in the main thread in reaction to a log_writer_reopen to change
 * the destination LogProtoClient instance. It needs to be ran in the main
 * thread as it reregisters the watches associated with the main
 * thread. */
void
log_writer_reopen_deferred(gpointer s)
{
  gpointer *args = (gpointer *) s;
  LogWriter *self = args[0];
  LogProtoClient *proto = args[1];

  if (!proto)
    {
      iv_validate_now();

      self->reopen_timer.expires = iv_now;
      self->reopen_timer.expires.tv_sec += self->options->time_reopen;

      if (iv_timer_registered(&self->reopen_timer))
        iv_timer_unregister(&self->reopen_timer);

      iv_timer_register(&self->reopen_timer);
    }

  init_sequence_number(&self->seq_num);

  if (self->io_job.working)
    {
      /* NOTE: proto can be NULL but it is present... */
      log_writer_set_pending_proto(self, proto, TRUE);
      return;
    }

  log_writer_stop_watches(self);
  log_writer_stop_idle_timer(self);

  if (self->partial_write)
    {
      log_queue_rewind_backlog_all(self->queue);
    }
  log_writer_free_proto(self);
  log_writer_set_proto(self, proto);

  if (proto)
    log_writer_start_watches(self);
}

/*
 * This function can be called from any threads, from the main thread
 * as well as I/O worker threads. It takes care about going to the
 * main thread to actually switch LogProtoClient under this writer.
 *
 * The writer may still be operating, (e.g. log_pipe_deinit/init is
 * not needed).
 *
 * In case we're running in a non-main thread, then by the time this
 * function returns, the reopen has finished. In case it is called
 * from the main thread, this function may defer updating self->proto
 * until the worker thread has finished. The reason for this
 * difference is:
 *
 *   - if LogWriter is busy, then updating the LogProtoClient instance is
 *     deferred to log_writer_work_finished(), but that runs in the
 *     main thread.
 *
 *   - normally, even this deferred update is waited for, but in case
 *     we're in the main thread, we can't block.
 *
 * This situation could probably be improved, maybe the synchonous
 * return of log_writer_reopen() is not needed by call sites, but I
 * was not sure, and right before release I didn't want to take the
 * risky approach.
 */
void
log_writer_reopen(LogWriter *s, LogProtoClient *proto)
{
  LogWriter *self = (LogWriter *) s;
  gpointer args[] = { s, proto };

  main_loop_call((MainLoopTaskFunc) log_writer_reopen_deferred, args, TRUE);

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
log_writer_set_options(LogWriter *self, LogPipe *control, LogWriterOptions *options,
                       const gchar *stats_id, const gchar *stats_instance)
{
  self->control = control;
  self->options = options;

  if (control)
    self->super.expr_node = control->expr_node;

  if (self->stats_id)
    g_free(self->stats_id);
  self->stats_id = stats_id ? g_strdup(stats_id) : NULL;

  if (self->stats_instance)
    g_free(self->stats_instance);
  self->stats_instance = stats_instance ? g_strdup(stats_instance) : NULL;
}

LogWriter *
log_writer_new(guint32 flags, GlobalConfig *cfg)
{
  LogWriter *self = g_new0(LogWriter, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.init = log_writer_init;
  self->super.deinit = log_writer_deinit;
  self->super.queue = log_writer_queue;
  self->super.free_fn = log_writer_free;
  self->flags = flags;
  self->line_buffer = g_string_sized_new(128);
  self->pollable_state = -1;
  init_sequence_number(&self->seq_num);

  log_writer_init_watches(self);
  g_static_mutex_init(&self->suppress_lock);
  g_static_mutex_init(&self->pending_proto_lock);
  self->pending_proto_cond = g_cond_new();

  return self;
}

void
log_writer_options_defaults(LogWriterOptions *options)
{
  options->template = NULL;
  options->flush_lines = -1;
  log_template_options_defaults(&options->template_options);
  options->time_reopen = -1;
  options->suppress = -1;
  options->padding = 0;
  options->mark_mode = MM_GLOBAL;
  options->mark_freq = -1;
  host_resolve_options_defaults(&options->host_resolve_options);
}

void
log_writer_options_set_template_escape(LogWriterOptions *options, gboolean enable)
{
  if (options->template && options->template->def_inline)
    {
      log_template_set_escape(options->template, enable);
    }
  else
    {
      msg_error("Macro escaping can only be specified for inline templates");
    }
}

void
log_writer_options_set_mark_mode(LogWriterOptions *options, const gchar *mark_mode)
{
  options->mark_mode = cfg_lookup_mark_mode(mark_mode);
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
log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, guint32 option_flags)
{
  if (options->initialized)
    return;

  log_template_options_init(&options->template_options, cfg);
  host_resolve_options_init(&options->host_resolve_options, &cfg->host_resolve_options);
  log_proto_client_options_init(&options->proto_options.super, cfg);
  options->options |= option_flags;

  if (options->flush_lines == -1)
    options->flush_lines = cfg->flush_lines;
  if (options->suppress == -1)
    options->suppress = cfg->suppress;
  if (options->time_reopen == -1)
    options->time_reopen = cfg->time_reopen;
  options->file_template = log_template_ref(cfg->file_template);
  options->proto_template = log_template_ref(cfg->proto_template);
  if (cfg->threaded)
    options->options |= LWO_THREADED;
  /* per-destination MARK messages */
  if (options->mark_mode == MM_GLOBAL)
    {
      /* get the global option */
      options->mark_mode = cfg->mark_mode;
    }
  if (options->mark_freq == -1)
    {
      /* not initialized, use the global mark freq */
      options->mark_freq = cfg->mark_freq;
    }

  options->initialized = TRUE;
}

void
log_writer_options_destroy(LogWriterOptions *options)
{
  log_template_options_destroy(&options->template_options);
  host_resolve_options_destroy(&options->host_resolve_options);
  log_proto_client_options_destroy(&options->proto_options.super);
  log_template_unref(options->template);
  log_template_unref(options->file_template);
  log_template_unref(options->proto_template);
  options->initialized = FALSE;
}

gint
log_writer_options_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "syslog-protocol") == 0)
    return LWO_SYSLOG_PROTOCOL;
  if (strcmp(flag, "no-multi-line") == 0)
    return LWO_NO_MULTI_LINE;
  if (strcmp(flag, "threaded") == 0)
    return LWO_THREADED;
  if (strcmp(flag, "ignore-errors") == 0)
    return LWO_IGNORE_ERRORS;
  msg_error("Unknown dest writer flag", evt_tag_str("flag", flag));
  return 0;
}
