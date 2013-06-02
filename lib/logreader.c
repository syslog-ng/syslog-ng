/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "tags.h"
#include "cfg-parser.h"
#include "timeutils.h"
#include "compat.h"
#include "mainloop.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iv.h>
#include <iv_work.h>

/**
 * FIXME: LogReader has grown big enough that it is difficult to
 * maintain it. The root of the problem is a design issue, instead of
 * having each source driver derive from LogReader, they use a
 * log-reader object. As most of the common options are implemented
 * here, each driver specific hack was added to logreader, making it
 * much more difficult to read and modify.
 *
 * Examples: 
 * 
 *  - file position tracking, follow-freq and immediate-check are file
 *    source specific
 *  - name resolution and peer addresses applies only to network based sources
 *  - etc.
 *
 * The solution I have in mind is to get rid of LogDriver (which is
 * very thin anyway, basically is only a "next" pointer in the linked
 * list of an s/dgroup, then convert sgroup and dgroup to use GList or
 * GPtrArray, this way sgroup/dgroup would not rely on LogDriver
 * instances, it'd simple use LogPipes. Then, each of the source
 * drivers would be derived from LogReader (probably after a rename to
 * reflect the change), and each driver specific features would be
 * added by using virtual methods instead of using a monolithic class.
 *
 * Of course a similar change can be applied to LogWriters as well.
 **/

struct _LogReader
{
  LogSource super;
  LogProtoServer *proto;
  gboolean immediate_check;
  gboolean waiting_for_preemption;
  LogPipe *control;
  LogReaderOptions *options;
  GSockAddr *peer_addr;
  gchar *follow_filename;
  ino_t inode;
  gint64 size;

  /* NOTE: these used to be LogReaderWatch members, which were merged into
   * LogReader with the multi-thread refactorization */

  struct iv_fd fd_watch;
  struct iv_timer follow_timer;
  struct iv_task restart_task;
  struct iv_event schedule_wakeup;
  MainLoopIOWorkerJob io_job;
  gboolean suspended:1;
  gint pollable_state;
  gint notify_code;
  gboolean pending_proto_present;
  GCond *pending_proto_cond;
  GStaticMutex pending_proto_lock;
  LogProtoServer *pending_proto;
};

static gboolean log_reader_fetch_log(LogReader *self);

static gboolean log_reader_start_watches(LogReader *self);
static void log_reader_stop_watches(LogReader *self);
static void log_reader_update_watches(LogReader *self);


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
      if (self->proto)
        log_proto_server_free(self->proto);

      self->proto = self->pending_proto;
      self->pending_proto = NULL;
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
      log_reader_start_watches(self);
    }
  log_pipe_unref(&self->super.super);
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
log_reader_io_process_input(gpointer s)
{
  LogReader *self = (LogReader *) s;

  log_reader_stop_watches(self);
  log_pipe_ref(&self->super.super);
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
      if (!main_loop_io_worker_job_quit())
        {
          log_reader_work_perform(s);
          log_reader_work_finished(s);
        }
    }
}

/* follow timer callback. Check if the file has new content, or deleted or
 * moved.  Ran every follow_freq seconds.  */
static void
log_reader_io_follow_file(gpointer s)
{
  LogReader *self = (LogReader *) s;
  struct stat st, followed_st;
  off_t pos = -1;
  gint fd = log_proto_server_get_fd(self->proto);

  msg_trace("Checking if the followed file has new lines",
            evt_tag_str("follow_filename", self->follow_filename),
            NULL);
  if (fd >= 0)
    {
      pos = lseek(fd, 0, SEEK_CUR);
      if (pos == (off_t) -1)
        {
          msg_error("Error invoking seek on followed file",
                    evt_tag_errno("error", errno),
                    NULL);
          goto reschedule;
        }

      if (fstat(fd, &st) < 0)
        {
          if (errno == ESTALE)
            {
              msg_trace("log_reader_fd_check file moved ESTALE",
                        evt_tag_str("follow_filename", self->follow_filename),
                        NULL);
              log_pipe_notify(self->control, NC_FILE_MOVED, self);
              return;
            }
          else
            {
              msg_error("Error invoking fstat() on followed file",
                        evt_tag_errno("error", errno),
                        NULL);
              goto reschedule;
            }
        }

      msg_trace("log_reader_fd_check",
                evt_tag_int("pos", pos),
                evt_tag_int("size", st.st_size),
                NULL);

      if (pos < st.st_size || !S_ISREG(st.st_mode))
        {
          /* we have data to read */
          log_reader_io_process_input(s);
          return;
        }
      else if (pos == st.st_size)
        {
          /* we are at EOF */
          log_pipe_notify(self->control, NC_FILE_EOF, self);
        }
      else if (pos > st.st_size)
        {
          /* the last known position is larger than the current size of the file. it got truncated. Restart from the beginning. */
          log_pipe_notify(self->control, NC_FILE_MOVED, self);

          /* we may be freed by the time the notification above returns */
          return;
        }
    }

  if (self->follow_filename)
    {
      if (stat(self->follow_filename, &followed_st) != -1)
        {
          if (fd < 0 || (st.st_ino != followed_st.st_ino && followed_st.st_size > 0))
            {
              msg_trace("log_reader_fd_check file moved eof",
                        evt_tag_int("pos", pos),
                        evt_tag_int("size", followed_st.st_size),
                        evt_tag_str("follow_filename", self->follow_filename),
                        NULL);
              /* file was moved and we are at EOF, follow the new file */
              log_pipe_notify(self->control, NC_FILE_MOVED, self);
              /* we may be freed by the time the notification above returns */
              return;
            }
        }
      else
        {
          msg_verbose("Follow mode file still does not exist",
                      evt_tag_str("filename", self->follow_filename),
                      NULL);
        }
    }
 reschedule:
  log_reader_update_watches(self);
}

static void
log_reader_init_watches(LogReader *self)
{
  IV_FD_INIT(&self->fd_watch);
  self->fd_watch.cookie = self;

  IV_TIMER_INIT(&self->follow_timer);
  self->follow_timer.cookie = self;
  self->follow_timer.handler = log_reader_io_follow_file;

  IV_TASK_INIT(&self->restart_task);
  self->restart_task.cookie = self;
  self->restart_task.handler = log_reader_io_process_input;

  IV_EVENT_INIT(&self->schedule_wakeup);
  self->schedule_wakeup.cookie = self;
  self->schedule_wakeup.handler = log_reader_wakeup_triggered;

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = (void (*)(void *)) log_reader_work_perform;
  self->io_job.completion = (void (*)(void *)) log_reader_work_finished;
}

/* NOTE: the return value is only used during initialization, and it is not
 * expected that it'd change once it returns success */
static gboolean
log_reader_start_watches(LogReader *self)
{
  gint fd;
  GIOCondition cond;

  log_proto_server_prepare(self->proto, &fd, &cond);

  if (self->options->follow_freq > 0)
    {
      /* follow freq specified (only the file source does that, go into timed polling */

      /* NOTE: the fd may not be set here, as it may not have been opened yet */
      iv_timer_register(&self->follow_timer);
    }
  else
    {
      if (fd < 0)
        {
          msg_error("In order to poll non-yet-existing files, follow_freq() must be set",
                    NULL);
          return FALSE;
        }

      /* we have an FD, it is possible to poll it, register it  */
      self->fd_watch.fd = fd;
      if (self->pollable_state < 0)
        {
          self->pollable_state = !iv_fd_register_try(&self->fd_watch);
        }
      else if (self->pollable_state > 0)
        {
          iv_fd_register(&self->fd_watch);
        }

      if (self->pollable_state == 0)
        {
          msg_error("Unable to determine how to monitor this fd, follow_freq() not set and it is not possible to poll it with the current ivykis polling method, try changing IV_EXCLUDE_POLL_METHOD environment variable",
                    evt_tag_int("fd", fd),
                    NULL);
          return FALSE;
        }
    }

  log_reader_update_watches(self);
  return TRUE;
}


static void
log_reader_stop_watches(LogReader *self)
{
  if (iv_fd_registered(&self->fd_watch))
    iv_fd_unregister(&self->fd_watch);
  if (iv_timer_registered(&self->follow_timer))
    iv_timer_unregister(&self->follow_timer);
  if (iv_task_registered(&self->restart_task))
    iv_task_unregister(&self->restart_task);
}

static void
log_reader_update_watches(LogReader *self)
{
  gint fd;
  GIOCondition cond;
  gboolean free_to_send;

  main_loop_assert_main_thread();
  
  self->suspended = FALSE;
  free_to_send = log_source_free_to_send(&self->super);
  if (!free_to_send ||
      self->immediate_check ||
      log_proto_server_prepare(self->proto, &fd, &cond))
    {
      /* we disable all I/O related callbacks here because we either know
       * that we can continue (e.g.  immediate_check == TRUE) or we know
       * that we can't continue even if data would be available (e.g.
       * free_to_send == FALSE)
       */

      self->immediate_check = FALSE;
      if (iv_fd_registered(&self->fd_watch))
        {
          iv_fd_set_handler_in(&self->fd_watch, NULL);
          iv_fd_set_handler_out(&self->fd_watch, NULL);

          /* we disable the error handler too, as it might be
           * triggered even when we don't want to read data
           * (e.g. log_source_free_to_send() is FALSE).
           *
           * And at least on Linux, it may happen that EPOLLERR is
           * set, while there's still data in the socket buffer.  Thus
           * in reaction to an EPOLLERR, we could possibly send
           * further messages without validating the
           * log_source_free_to_send() would allow us to, potentially
           * overflowing our window (and causing a failed assertion in
           * log_source_queue().
           */

          iv_fd_set_handler_err(&self->fd_watch, NULL);
        }

      if (iv_timer_registered(&self->follow_timer))
        iv_timer_unregister(&self->follow_timer);

      if (free_to_send)
        {
          /* we have data in our input buffer, we need to start working
           * on it immediately, without waiting for I/O events */
          if (!iv_task_registered(&self->restart_task))
            {
              iv_task_register(&self->restart_task);
            }
        }
      else
        {
          self->suspended = TRUE;
        }
      return;
    }

  if (iv_fd_registered(&self->fd_watch))
    {
      /* this branch is executed when our fd is connected to a non-file
       * source (e.g. TCP, UDP socket). We set up I/O callbacks here.
       * files cannot be polled using epoll, as it causes an I/O error
       * (thus abort in ivykis).
       */
      if (cond & G_IO_IN)
        iv_fd_set_handler_in(&self->fd_watch, log_reader_io_process_input);
      else
        iv_fd_set_handler_in(&self->fd_watch, NULL);

      if (cond & G_IO_OUT)
        iv_fd_set_handler_out(&self->fd_watch, log_reader_io_process_input);
      else
        iv_fd_set_handler_out(&self->fd_watch, NULL);

      if (cond & (G_IO_IN + G_IO_OUT))
        iv_fd_set_handler_err(&self->fd_watch, log_reader_io_process_input);
      else
        iv_fd_set_handler_err(&self->fd_watch, NULL);

    }
  else
    {
      if (self->options->follow_freq > 0)
        {
          if (iv_timer_registered(&self->follow_timer))
            iv_timer_unregister(&self->follow_timer);
          iv_validate_now();
          self->follow_timer.expires = iv_now;
          timespec_add_msec(&self->follow_timer.expires, self->options->follow_freq);
          iv_timer_register(&self->follow_timer);
        }
      else
        {
          /* NOTE: we don't need to unregister the timer here as follow_freq
           * never changes during runtime, thus if ever it was registered that
           * also means that we go into the if branch above. */
        }
    }
}

static gboolean
log_reader_handle_line(LogReader *self, const guchar *line, gint length, GSockAddr *saddr)
{
  LogMessage *m;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  
  msg_debug("Incoming log entry", 
            evt_tag_printf("line", "%.*s", length, line),
            NULL);
  /* use the current time to get the time zone offset */
  m = log_msg_new((gchar *) line, length,
                  saddr,
                  &self->options->parse_options);

  log_msg_refcache_start_producer(m);
  if (!m->saddr && self->peer_addr)
    {
      m->saddr = g_sockaddr_ref(self->peer_addr);
    }

  log_pipe_queue(&self->super.super, m, &path_options);
  log_msg_refcache_stop();
  return log_source_free_to_send(&self->super);
}

/* returns: notify_code (NC_XXXX) or 0 for success */
static gint
log_reader_fetch_log(LogReader *self)
{
  GSockAddr *sa;
  gint msg_count = 0;
  gboolean may_read = TRUE;

  if (self->waiting_for_preemption)
    may_read = FALSE;

  /* NOTE: this loop is here to decrease the load on the main loop, we try
   * to fetch a couple of messages in a single run (but only up to
   * fetch_limit).
   */
  while (msg_count < self->options->fetch_limit && !main_loop_io_worker_job_quit())
    {
      const guchar *msg;
      gsize msg_len;
      LogProtoStatus status;

      msg = NULL;
      sa = NULL;

      /* NOTE: may_read is used to implement multi-read checking. It
       * is initialized to TRUE to indicate that the protocol is
       * allowed to issue a read(). If multi-read is disallowed in the
       * protocol, it resets may_read to FALSE after the first read was issued.
       */

      status = log_proto_server_fetch(self->proto, &msg, &msg_len, &sa, &may_read);
      switch (status)
        {
        case LPS_EOF:
        case LPS_ERROR:
          g_sockaddr_unref(sa);
          return status == LPS_ERROR ? NC_READ_ERROR : NC_CLOSE;
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

          if (!log_reader_handle_line(self, msg, msg_len, sa))
            {
              /* window is full, don't generate further messages */
              log_proto_server_queued(self->proto);
              g_sockaddr_unref(sa);
              break;
            }
        }
      log_proto_server_queued(self->proto);
      g_sockaddr_unref(sa);
    }
  if (self->options->flags & LR_PREEMPT)
    {
      if (log_proto_server_is_preemptable(self->proto))
        {
          self->waiting_for_preemption = FALSE;
          log_pipe_notify(self->control, NC_FILE_SKIP, self);
        }
      else
        {
          self->waiting_for_preemption = TRUE;
        }
    }
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
                evt_tag_str("format", self->options->parse_options.format),
                NULL);
      return FALSE;
    }
  if (!log_reader_start_watches(self))
    return FALSE;
  iv_event_register(&self->schedule_wakeup);

  return TRUE;
}

static gboolean
log_reader_deinit(LogPipe *s)
{
  LogReader *self = (LogReader *) s;
  
  main_loop_assert_main_thread();

  iv_event_unregister(&self->schedule_wakeup);
  log_reader_stop_watches(self);
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
  log_pipe_unref(self->control);
  g_sockaddr_unref(self->peer_addr);
  g_free(self->follow_filename);
  g_static_mutex_free(&self->pending_proto_lock);
  g_cond_free(self->pending_proto_cond);
  log_source_free(s);
}

void
log_reader_set_options(LogReader *s, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance)
{
  LogReader *self = (LogReader *) s;

  log_source_set_options(&self->super, &options->super, stats_level, stats_source, stats_id, stats_instance, (options->flags & LR_THREADED));

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;

  self->options = options;
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

  log_reader_stop_watches(self);
  if (self->io_job.working)
    {
      /* NOTE: proto can be NULL */
      self->pending_proto = proto;
      self->pending_proto_present = TRUE;
      return;
    }

  if (self->proto)
    log_proto_server_free(self->proto);

  self->proto = proto;

  if(proto)
    {
        log_reader_start_watches(self);
    }
}

void
log_reader_reopen(LogReader *self, LogProtoServer *proto, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance, gboolean immediate_check)
{
  gpointer args[] = { self, proto };

  log_source_deinit(&self->super.super);

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
  if (immediate_check)
    {
      log_reader_set_immediate_check(self);
    }
  log_reader_set_options(self, control, options, stats_level, stats_source, stats_id, stats_instance);
  log_reader_set_follow_filename(self, stats_instance);
  log_source_init(&self->super.super);
}

void
log_reader_set_follow_filename(LogReader *s, const gchar *follow_filename)
{
  /* try to free */
  LogReader *self = (LogReader *) s;
  g_free(self->follow_filename);
  self->follow_filename = g_strdup(follow_filename);
}

void
log_reader_set_peer_addr(LogReader *s, GSockAddr *peer_addr)
{
  LogReader *self = (LogReader *) s;
  self->peer_addr = g_sockaddr_ref(peer_addr);
}

LogReader *
log_reader_new(LogProtoServer *proto)
{
  LogReader *self = g_new0(LogReader, 1);

  log_source_init_instance(&self->super);
  self->super.super.init = log_reader_init;
  self->super.super.deinit = log_reader_deinit;
  self->super.super.free_fn = log_reader_free;
  self->super.wakeup = log_reader_wakeup;
  self->proto = proto;
  self->immediate_check = FALSE;
  self->pollable_state = -1;
  log_reader_init_watches(self);
  g_static_mutex_init(&self->pending_proto_lock);
  self->pending_proto_cond = g_cond_new();
  return self;
}

void 
log_reader_set_immediate_check(LogReader *s)
{
  LogReader *self = (LogReader*) s;
  
  self->immediate_check = TRUE;
}

void
log_reader_options_defaults(LogReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  log_proto_server_options_defaults(&options->proto_options.super);
  msg_format_options_defaults(&options->parse_options);
  options->fetch_limit = 10;
  options->follow_freq = -1; 
  if (configuration && cfg_is_config_version_older(configuration, 0x0300))
    {
      static gboolean warned;
      if (!warned)
        {
          msg_warning("WARNING: input: sources do not remove new-line characters from messages by default from " VERSION_3_0 ", please add 'no-multi-line' flag to your configuration if you want to retain this functionality",
                      NULL);
          warned = TRUE;
        }
      options->parse_options.flags |= LP_NO_MULTI_LINE;
    }
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

  if (options->follow_freq == -1)
    options->follow_freq = cfg->follow_freq;
  if (options->check_hostname == -1)
    options->check_hostname = cfg->check_hostname;
  if (options->check_hostname)
    {
      options->parse_options.flags |= LP_CHECK_HOSTNAME;
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
log_reader_options_process_flag(LogReaderOptions *options, gchar *flag)
{
  if (!msg_format_options_process_flag(&options->parse_options, flag))
    return cfg_process_flag(log_reader_flag_handlers, options, flag);
  return TRUE;
}
