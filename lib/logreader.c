/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include "logproto.h"
#include "misc.h"
#include "stats.h"
#include "tags.h"
#include "cfg-parser.h"
#include "timeutils.h"
#include "compat.h"
#include "mainloop.h"
#include "mainloop-call.h"
#include "mainloop-io-worker.h"
#include "str-format.h"
#include "versioning.h"
#include "ack_tracker.h"

#include <sys/types.h>
//#include <sys/socket.h>
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
#include <iv_event.h>
#include <regex.h>

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

/* In seconds */
#define MAX_MSG_TIMEOUT 10

struct _LogReader
{
  LogSource super;
  LogProto *proto;
  gboolean immediate_check;
  gboolean waiting_for_preemption;
  LogPipe *control;
  LogReaderOptions *options;
  GSockAddr *peer_addr;
  gchar *follow_filename;
  ino_t inode;
  gint64 size;

  struct iv_fd fd_watch;
  struct iv_timer follow_timer;
  struct iv_timer update_timer;
  struct iv_timer idle_timer;
  struct iv_timer wait_timer;
  struct iv_task restart_task;
  struct iv_task immediate_check_task;
  struct iv_event schedule_wakeup;
  MainLoopIOWorkerJob io_job;
  gboolean suspended:1;
  gint pollable_state;
  gint notify_code;
  /* Because of multiline processing logreader has to store the last read line which should start with the prefix */
  gchar *partial_message;
  /* store if we have to wait for a prefix, because last event was a garbage found */
  gboolean wait_for_prefix;
  gboolean flush;
  time_t last_msg_received;

  gboolean pending_proto_present;
  GCond *pending_proto_cond;
  GStaticMutex pending_proto_lock;
  LogProto *pending_proto;
  gboolean force_read;
  gboolean is_regular;
};

static gboolean log_reader_start_watches(LogReader *self);
static void log_reader_stop_watches(LogReader *self);
static void log_reader_update_watches(LogReader *self);
static void log_reader_free_proto(LogReader *self);
static void log_reader_set_proto(LogReader *self, LogProto *proto);
static void log_reader_set_pending_proto(LogReader *self, LogProto *proto);

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

      log_reader_free_proto(self);
      log_reader_set_proto(self, self->pending_proto);
      log_reader_set_pending_proto(self, NULL);

      g_cond_signal(self->pending_proto_cond);
      g_static_mutex_unlock(&self->pending_proto_lock);
    }

  if (self->notify_code == 0)
    {
      if (self->super.super.flags & PIF_INITIALIZED)
        {
          /* reenable polling the source assuming that we're still in
           * business (e.g. the reader hasn't been uninitialized) */

          log_reader_start_watches(self);
        }
    }
  else
    {
      log_pipe_notify(self->control, &self->super.super, self->notify_code, self);
    }
  log_pipe_unref(&self->super.super);
}

void log_reader_restart(LogPipe *s)
{
  LogReader *self = (LogReader *)s;
  log_reader_update_watches(self);
}

static void
log_reader_wakeup_triggered(gpointer s)
{
  LogReader *self = (LogReader *) s;
  if (!self->io_job.working)
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
  if (!main_loop_worker_job_quit())
    {
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
          log_reader_work_perform(s);
          log_reader_work_finished(s);
        }
    }
}

static void
log_reader_flush_buffer(LogReader *reader,gboolean reset_state)
{
  reader->flush = TRUE;
  log_reader_fetch_log(reader);
  reader->flush = FALSE;
  if(reset_state)
    {
      log_proto_reset_state(reader->proto);
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
  gint fd = log_proto_get_fd(self->proto);

  msg_debug("Checking if the followed file has new lines",
            evt_tag_str("follow_filename", self->follow_filename),
            NULL);
  if (fd >= 0)
    {
      pos = lseek(fd, 0, SEEK_CUR);
      if (pos == (off_t) -1)
        {
          msg_error("Error invoking seek on followed file",
                    evt_tag_errno("error", errno),
                    evt_tag_id(MSG_LOGREADER_SEEK_ERROR),
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
              log_reader_flush_buffer(self, TRUE);
              log_pipe_notify(self->control, &self->super.super, NC_FILE_MOVED, self);
              return;
            }
          else
            {
              msg_error("Error invoking fstat() on followed file",
                        evt_tag_errno("error", errno),
                        evt_tag_id(MSG_LOGREADER_FSTAT_ERROR),
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
          self->size = st.st_size;
          log_reader_io_process_input(s);
          return;
        }
      else if (pos > st.st_size)
        {
          /* the last known position is larger than the current size of the file. it got truncated. Restart from the beginning. */
          log_reader_flush_buffer(self, TRUE);
          log_pipe_notify(self->control, &self->super.super, NC_FILE_MOVED, self);

          /* we may be freed by the time the notification above returns */
          return;
        }
    }

  if (self->follow_filename)
    {
      if (stat(self->follow_filename, &followed_st) != -1)
        {
         gboolean has_eof_moved = FALSE;
#ifdef _WIN32
         DWORD attributes = GetFileAttributes(self->follow_filename);
         has_eof_moved = (fd < 0 || (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_ACCESS_DENIED));
#else
         has_eof_moved = (fd < 0 || (st.st_ino != followed_st.st_ino && followed_st.st_size > 0));
#endif
          if (has_eof_moved)
            {
              msg_trace("log_reader_fd_check file moved eof",
                        evt_tag_int("pos", pos),
                        evt_tag_int("size", followed_st.st_size),
                        evt_tag_str("follow_filename", self->follow_filename),
                        NULL);
              /* file was moved and we are at EOF, follow the new file */
              log_reader_flush_buffer(self, TRUE);
              log_pipe_notify(self->control, &self->super.super, NC_FILE_MOVED, self);
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
log_reader_check_file(gpointer s)
{
  LogReader *self = (LogReader *) s;
  struct stat st;
  off_t pos = -1;
  gint fd = log_proto_get_fd(self->proto);

  if (fd >= 0)
    {
      pos = lseek(fd, 0, SEEK_CUR);
      if (pos == (off_t) -1)
        {
          return;
        }

      if (fstat(fd, &st) >= 0 && pos == st.st_size)
        {
          /* we are at EOF */
          /* We are the end of the file, so let's see the timeout */
          if((self->proto->is_multi_line) &&
             (self->last_msg_received != 0) &&
             (self->last_msg_received + MAX_MSG_TIMEOUT < cached_g_current_time_sec()))
            {
              self->last_msg_received = 0;
              log_reader_flush_buffer(self, FALSE);
            }
          gboolean is_valid = TRUE;
          self->size = st.st_size;
          log_pipe_notify(self->control, &self->super.super, NC_FILE_EOF, &is_valid);
        }
    }
  return;
}

static void
log_reader_restart_task_handler(gpointer s)
{
  LogReader *self = (LogReader *)s;
  struct stat st;
  gint fd;

  g_assert(self->proto);
  fd = log_proto_get_fd(self->proto);
  if (fd >= 0)
    {
      if (self->follow_filename && fstat(fd, &st) >= 0)
        {
          self->size = st.st_size;
        }
      log_reader_io_process_input(s);
    }
}

static void
log_reader_idle_timeout(LogReader *self)
{
  msg_error("Client response time elapsed, close the connection",evt_tag_id(MSG_LOGREADER_RESPONSE_TIMEOUT),NULL);
  log_pipe_notify(self->control, &self->super.super, NC_CLOSE, self);
}

static void
log_reader_wait_timeout(LogReader *self)
{
  msg_error("Wait time elapsed, close the connection",evt_tag_id(MSG_LOGREADER_RESPONSE_TIMEOUT),NULL);
  log_proto_reset_state(self->proto);
  log_pipe_notify(self->control, &self->super.super, NC_CLOSE, self);
}

static void
log_reader_init_watches(LogReader *self)
{
  gint fd;
  GIOCondition cond;
  gint idle_timeout = -1;

  log_proto_prepare(self->proto, &fd, &cond, &idle_timeout);

  IV_FD_INIT(&self->fd_watch);
  self->fd_watch.cookie = self;

  IV_TIMER_INIT(&self->follow_timer);
  self->follow_timer.cookie = self;
  self->follow_timer.handler = log_reader_io_follow_file;

  IV_TIMER_INIT(&self->update_timer);
  self->update_timer.cookie = self;
  self->update_timer.handler = (void (*)(void *))log_reader_update_watches;

  IV_TIMER_INIT(&self->idle_timer);
  self->idle_timer.cookie = self;
  self->idle_timer.handler = (void (*)(void *))log_reader_idle_timeout;

  IV_TIMER_INIT(&self->wait_timer);
  self->wait_timer.cookie = self;
  self->wait_timer.handler = (void (*)(void *))log_reader_wait_timeout;

  IV_TASK_INIT(&self->restart_task);
  self->restart_task.cookie = self;
  self->restart_task.handler = log_reader_restart_task_handler;

  IV_TASK_INIT(&self->immediate_check_task);
  self->immediate_check_task.cookie = self;
  self->immediate_check_task.handler = log_reader_check_file;

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
  gint idle_timeout = -1;

  log_proto_prepare(self->proto, &fd, &cond, &idle_timeout);

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
                    evt_tag_id(MSG_READER_CANT_POLL),
                    NULL);
          return FALSE;
        }
      self->fd_watch.fd = fd;
      if (self->pollable_state < 0)
        {
          if (iv_fd_register_try(&self->fd_watch) == 0)
            self->pollable_state = 1;
          else
            self->pollable_state = 0;
        }
      /* we have an FD, it is possible to poll it, register it */
      else if (self->pollable_state > 0)
        {
          iv_fd_register(&self->fd_watch);
        }
      if (self->pollable_state == 0)
        {
          msg_error("Unable to determine how to monitor this fd, follow_freq() not set and it is not possible to poll it with the current ivykis polling method, try changing IV_EXCLUDE_POLL_METHOD environment variable",
                    evt_tag_int("fd", fd),
                    evt_tag_id(MSG_READER_CANT_POLL),
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
  if (iv_task_registered(&self->immediate_check_task))
    iv_task_unregister(&self->immediate_check_task);
  if (iv_timer_registered(&self->idle_timer))
    iv_timer_unregister(&self->idle_timer);
  if (iv_timer_registered(&self->update_timer))
    iv_timer_unregister(&self->update_timer);
  if (iv_timer_registered(&self->wait_timer))
    iv_timer_unregister(&self->wait_timer);
}

static void
log_reader_update_watches(LogReader *self)
{
  gint fd;
  GIOCondition cond;
  gboolean free_to_send;
  gboolean prepare_result;
  gint idle_timeout = -1;

  main_loop_assert_main_thread();
  
  self->suspended = FALSE;
  free_to_send = log_source_free_to_send(&self->super);
  prepare_result = log_proto_prepare(self->proto, &fd, &cond, &idle_timeout);

  if (self->force_read)
    {
      self->force_read = FALSE;
      if (!iv_task_registered(&self->restart_task))
        iv_task_register(&self->restart_task);
    }

  if (!free_to_send ||
      self->immediate_check ||
      prepare_result)
    {
      /* we disable all I/O related callbacks here because we either know
       * that we can continue (e.g.  immediate_check == TRUE) or we know
       * that we can't continue even if data would be available (e.g.
       * free_to_send == FALSE)
       */

      gboolean old_immediate_check = self->immediate_check;
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

        /*
         * This means, that reader has to wait for an event what is observed by the proto
         * In this case the reader wait 1 sec and than run the update_watches will be run again
         */
      if (prepare_result && cond == G_IO_OUT)
        {
          if (iv_timer_registered(&self->update_timer))
            iv_timer_unregister(&self->update_timer);
          iv_validate_now();
          self->update_timer.expires = iv_now;
          self->update_timer.expires.tv_sec += 1;

          iv_timer_register(&self->update_timer);
          if (idle_timeout != -1)
            {
              if (!iv_timer_registered(&self->wait_timer))
                {
                  self->wait_timer.expires = iv_now;
                  self->wait_timer.expires.tv_sec += idle_timeout;
                  iv_timer_register(&self->wait_timer);
                }
            }
          return;
        }

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
          self->immediate_check = old_immediate_check;
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
      {
#ifdef _WIN32
       if (!iv_task_registered(&self->restart_task))
       {
         iv_task_register(&self->restart_task);
       }
#else
        iv_fd_set_handler_out(&self->fd_watch, log_reader_io_process_input);
#endif
      }
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

          /*
           * we should detect the end of file as soon as possible
           * to change to the next file in case of wildcard file sources
          */
          if (!iv_task_registered(&self->immediate_check_task))
            {
              iv_task_register(&self->immediate_check_task);
            }
        }
      else
        {
          /* NOTE: we don't need to unregister the timer here as follow_freq
           * never changes during runtime, thus if ever it was registered that
           * also means that we go into the if branch above. */
        }
    }
  if (idle_timeout != -1)
    {
      if (iv_timer_registered(&self->idle_timer))
            iv_timer_unregister(&self->idle_timer);
      iv_validate_now();
      self->idle_timer.expires = iv_now;
      self->idle_timer.expires.tv_sec += idle_timeout;
      iv_timer_register(&self->idle_timer);
    }
}

static gboolean
log_reader_handle_line(LogReader *self, const guchar *line, gint length, GSockAddr *saddr, Bookmark *bookmark)
{
  LogMessage *m;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  GString *converted;
  guint64 pos=0;
  NVHandle handle;

  /*
   * skip the remaning '\r' and '\n' char in case the multi line garbage is on
   * and the garbage processing took place
   * in several steps (file writing in steps)
   */
  while((length > 0) && ((line[0] == '\r') || (line[0] == '\n')))
    {
      ++line;
      --length;
    }

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

  if (self->is_regular && self->size == 0)
    {
      struct stat st;
      gint fd = log_proto_get_fd(self->proto);
      if (fd >= 0)
        {
          if (fstat(fd, &st) >= 0)
            {
              self->size = st.st_size;
            }
        }
    }

  if (self->proto->get_pending_pos && self->size > 0)
    {
      converted = g_string_sized_new (25);
      pos = self->proto->get_pending_pos(self->proto, bookmark);
      handle = log_msg_get_value_handle(SDATA_FILE_NAME);
      if(self->follow_filename && handle)
        log_msg_set_value(m, handle, self->follow_filename, strlen(self->follow_filename));

      handle = log_msg_get_value_handle(SDATA_FILE_POS);
      if (handle)
        {
          if (pos != 0)
            {
              format_uint64_padded(converted, 0, 0, 10, pos);
              log_msg_set_value(m, handle, converted->str, converted->len);
            }
          else
            {
              log_msg_set_value(m, handle, "0", 1);
            }
         }

      handle = log_msg_get_value_handle(SDATA_FILE_SIZE);
      if (handle)
        {
          g_assert(self->size);
          g_string_set_size(converted, 0);
          format_uint64_padded(converted, 0, 0, 10, self->size);
          log_msg_set_value(m, handle, converted->str, converted->len);
        }

      g_string_free(converted, TRUE);
    }
  log_pipe_queue(&self->super.super, m, &path_options);
  log_msg_refcache_stop();

  return log_source_free_to_send(&self->super);
}

static inline gint
log_reader_process_handshake(LogReader *self)
{
  LogProtoStatus status;

  status = log_proto_handshake(self->proto);
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

/* returns: notify_code (NC_XXXX) or 0 for success */
gint
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
  if (log_proto_handshake_in_progress(self->proto))
    {
      return log_reader_process_handshake(self);
    }

  while (msg_count < self->options->fetch_limit && !main_loop_worker_job_quit())
    {
      Bookmark *bookmark;
      LogProtoStatus status;
      const guchar *msg;
      gsize msg_len;

      msg = NULL;
      sa = NULL;

      /* NOTE: may_read is used to implement multi-read checking. It
       * is initialized to TRUE to indicate that the protocol is
       * allowed to issue a read(). If multi-read is disallowed in the
       * protocol, it resets may_read to FALSE after the first read was issued.
       */

      // TODO: FetchedData{msg, msg_len, sa, bookmark};
      bookmark = ack_tracker_request_bookmark(self->super.ack_tracker);
      status = log_proto_fetch(self->proto, &msg, &msg_len, &sa, &may_read, self->flush, bookmark);
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
      self->last_msg_received = cached_g_current_time_sec();
      if (!msg)
        {
          /* no more messages for now */
          break;
        }
      if (msg_len > 0 || (self->options->flags & LR_EMPTY_LINES))
        {
          msg_count++;
          if (!log_reader_handle_line(self, msg, msg_len, sa, bookmark))
            {
              /* window is full, don't generate further messages */
              g_sockaddr_unref(sa);
              break;
            }
        }

      g_sockaddr_unref(sa);
    }
  if (msg_count == self->options->fetch_limit)
    self->immediate_check = TRUE;
  if (self->options->flags & LR_PREEMPT)
    {
      if (log_proto_is_preemptable(self->proto))
        {
          self->waiting_for_preemption = FALSE;
          if (msg_count > 0)
            {
              return NC_FILE_SKIP;
            }
        }
      else
        {
          self->waiting_for_preemption = TRUE;
        }
    }
  return 0;
}

static gboolean
log_reader_init(LogPipe *s)
{
  LogReader *self = (LogReader *) s;
  GlobalConfig *cfg;
  if (!log_source_init(s))
    return FALSE;
  cfg = log_pipe_get_config(s);
  g_assert(cfg);

  self->super.super.cfg = cfg;

  /* check for new data */
  if (self->options->padding)
    {
      if (self->options->msg_size < self->options->padding)
        {
          msg_error("Buffer is too small to hold padding number of bytes",
                    evt_tag_int("padding", self->options->padding),
                    evt_tag_int("msg_size", self->options->msg_size),
                    NULL);
          return FALSE;
        }
    }
  if (self->options->text_encoding)
    {
      if (!log_proto_set_encoding(self->proto, self->options->text_encoding))
        {
          msg_error("Unknown character set name specified",
                    evt_tag_str("encoding", self->options->text_encoding),
                    evt_tag_id(MSG_LOGREADER_UNKNOWN_ENCODING),
                    NULL);
          return FALSE;
        }
    }
  if (!self->options->parse_options.format_handler)
    {
      msg_error("Unknown format plugin specified",
                evt_tag_str("format", self->options->parse_options.format),
                evt_tag_id(MSG_LOGREADER_UNKNOWN_FORMAT),
                NULL);
      return FALSE;
    }

#ifdef _WIN32
  self->force_read = TRUE;
#endif

  self->is_regular = FALSE;
  if (self->proto)
    {
      gint fd = log_proto_get_fd(self->proto);
      if (fd >= 0)
        {
          self->is_regular = is_file_regular(fd);
        }
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

  log_reader_free_proto(self);
  log_pipe_unref(self->control);
  g_sockaddr_unref(self->peer_addr);
  g_free(self->follow_filename);
  g_static_mutex_free(&self->pending_proto_lock);
  g_cond_free(self->pending_proto_cond);
  log_source_free(s);
}

void
log_reader_set_options(LogPipe *s, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance, LogProtoOptions *proto_options)
{
  LogReader *self = (LogReader *) s;

  gboolean pos_tracked = ((self->proto != NULL) && log_proto_is_position_tracked(self->proto));

  log_source_set_options(&self->super, &options->super, stats_level, stats_source, stats_id, stats_instance, (options->flags & LR_THREADED), pos_tracked);

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;

  self->options = options;
  if (self->proto)
    log_proto_set_options(self->proto,proto_options);
}

/* run in the main thread in reaction to a log_reader_reopen to change
 * the source LogProto instance. It needs to be ran in the main
 * thread as it reregisters the watches associated with the main
 * thread. */
void
log_reader_reopen_deferred(gpointer s)
{
  gpointer *args = (gpointer *) s;
  LogReader *self = args[0];
  LogProto *proto = args[1];
  gboolean *immediate_check = args[2];

  log_reader_stop_watches(self);
  if (self->io_job.working)
    {
      log_reader_set_pending_proto(self, proto);
      return;
    }

  log_reader_free_proto(self);
  log_reader_set_proto(self, proto);

  self->is_regular = FALSE;
  if(proto)
    {
      gint fd = log_proto_get_fd(proto);
      if (fd >= 0)
        {
          self->is_regular = is_file_regular(fd);
        }

      if (*immediate_check)
        {
          log_reader_set_immediate_check(&self->super.super);
        }
      log_reader_start_watches(self);
    }
}

void
log_reader_reopen(LogPipe *s, LogProto *proto, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance, gboolean immediate_check, LogProtoOptions *proto_options)
{
  LogReader *self = (LogReader *) s;
  gpointer args[] = { s, proto, &immediate_check };
  log_source_deinit(s);

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
  log_reader_set_options(s, control, options, stats_level, stats_source, stats_id, stats_instance, proto_options);
  log_reader_set_follow_filename(s, stats_instance);
  log_source_init(s);
}

void
log_reader_set_follow_filename(LogPipe *s, const gchar *follow_filename)
{
  /* try to free */
  LogReader *self = (LogReader *) s;
  g_free(self->follow_filename);
  self->follow_filename = g_strdup(follow_filename);
}

void
log_reader_set_peer_addr(LogPipe *s, GSockAddr *peer_addr)
{
  LogReader *self = (LogReader *) s;
  self->peer_addr = g_sockaddr_ref(peer_addr);
}

static void
log_reader_free_proto(LogReader *self)
{
  if (self->proto)
    {
      log_proto_free(self->proto);
      log_reader_set_proto(self, NULL);
    }
}

static void
log_reader_set_proto(LogReader *self, LogProto *proto)
{
  if (proto != NULL)
    {
      self->proto = proto;
      self->super.is_external_ack_required = proto->flags & LPBS_EXTERNAL_ACK_REQUIRED;
    }
  else
    {
      self->proto = NULL;
      self->super.is_external_ack_required = FALSE;
    }
}

static void
log_reader_set_pending_proto(LogReader *self, LogProto *proto)
{
  if (proto != NULL)
    {
      self->pending_proto = proto;
      self->pending_proto_present = TRUE;
    }
  else
    {
      self->pending_proto = NULL;
      self->pending_proto_present = FALSE;
    }
}

LogPipe *
log_reader_new(LogProto *proto)
{
  LogReader *self = g_new0(LogReader, 1);

  log_source_init_instance(&self->super);
  self->super.super.init = log_reader_init;
  self->super.super.deinit = log_reader_deinit;
  self->super.super.free_fn = log_reader_free;
  self->super.wakeup = log_reader_wakeup;
  self->immediate_check = FALSE;
  self->pollable_state = -1;
  self->wait_for_prefix = FALSE;
  self->flush = FALSE;

  log_reader_set_proto(self, proto);

  log_reader_init_watches(self);
  self->last_msg_received = 0;
  g_static_mutex_init(&self->pending_proto_lock);
  self->pending_proto_cond = g_cond_new();
  return &self->super.super;
}

void 
log_reader_set_immediate_check(LogPipe *s)
{
  LogReader *self = (LogReader*) s;
  
  self->immediate_check = TRUE;
}

void
log_reader_options_defaults(LogReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  msg_format_options_defaults(&options->parse_options);
  options->padding = 0;
  options->fetch_limit = 10;
  options->msg_size = -1;
  options->follow_freq = -1; 
  options->text_encoding = NULL;
  if (!cfg_check_current_config_version(VERSION_VALUE_3_0))
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
 * NOTE: options_init and options_destroy are a bit weird, because their
 * invocation is not completely symmetric:
 *
 *   - init is called from driver init (e.g. affile_sd_init), 
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_sd_deinit)
 *
 * The reason:
 *   - when initializing the reloaded configuration fails for some reason,
 *     we have to fall back to the old configuration, thus we cannot dump
 *     the information stored in the Options structure.
 *
 * For the reasons above, init and destroy behave the following way:
 *
 *   - init is idempotent, it can be called multiple times without leaking
 *     memory, and without loss of information
 *   - destroy is only called once, when the options are indeed to be destroyed
 *
 * As init allocates memory, it has to take care about freeing memory
 * allocated by the previous init call (or it has to reuse those).
 *   
 */
void
log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  gchar *host_override, *program_override, *text_encoding, *format;
  MsgFormatHandler *format_handler;
  GArray *tags;

  recv_time_zone = options->parse_options.recv_time_zone;
  options->parse_options.recv_time_zone = NULL;
  recv_time_zone_info = options->parse_options.recv_time_zone_info;
  options->parse_options.recv_time_zone_info = NULL;
  text_encoding = options->text_encoding;
  options->text_encoding = NULL;

  /* NOTE: having to save super's variables is a crude hack, but I know of
   * no other way to do it in the scheme described above. Be sure that you
   * know what you are doing when you modify this code. */
  
  tags = options->super.tags;
  options->super.tags = NULL;
  host_override = options->super.host_override;
  options->super.host_override = NULL;
  program_override = options->super.program_override;
  options->super.program_override = NULL;

  format = options->parse_options.format;
  options->parse_options.format = NULL;
  format_handler = options->parse_options.format_handler;
  options->parse_options.format_handler = NULL;


  /***********************************************************************
   * PLEASE NOTE THIS. please read the comment at the top of the function
   ***********************************************************************/
  log_reader_options_destroy(options);

  options->parse_options.format = format;
  options->parse_options.format_handler = format_handler;

  options->super.host_override = host_override;
  options->super.program_override = program_override;
  options->super.tags = tags;
  
  options->parse_options.recv_time_zone = recv_time_zone;
  options->parse_options.recv_time_zone_info = recv_time_zone_info;
  options->text_encoding = text_encoding;
  options->parse_options.format = format;

  log_source_options_init(&options->super, cfg, group_name);
  msg_format_options_init(&options->parse_options, cfg);

  if (options->msg_size == -1)
    options->msg_size = cfg->log_msg_size;
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
  if (options->text_encoding)
    options->parse_options.flags |= LP_ASSUME_UTF8;
  if (cfg->threaded)
    options->flags |= LR_THREADED;
}

void
log_reader_options_destroy(LogReaderOptions *options)
{
  log_source_options_destroy(&options->super);
  msg_format_options_destroy(&options->parse_options);
  if (options->text_encoding)
    {
      g_free(options->text_encoding);
      options->text_encoding = NULL;
    }
}

CfgFlagHandler log_reader_flag_handlers[] =
{
  /* LogParseOptions */
  /* NOTE: underscores are automatically converted to dashes */
  { "no-parse",                   CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_NOPARSE },
  { "check-hostname",             CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_CHECK_HOSTNAME },
  { "syslog-protocol",            CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_SYSLOG_PROTOCOL },
  { "assume-utf8",                CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_ASSUME_UTF8 },
  { "validate-utf8",              CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_VALIDATE_UTF8 },
  { "no-multi-line",              CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_NO_MULTI_LINE },
  { "store-legacy-msghdr",        CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_STORE_LEGACY_MSGHDR },
  { "dont-store-legacy-msghdr", CFH_CLEAR, offsetof(LogReaderOptions, parse_options.flags), LP_STORE_LEGACY_MSGHDR },
  { "expect-hostname",            CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_EXPECT_HOSTNAME },
  { "no-hostname",              CFH_CLEAR, offsetof(LogReaderOptions, parse_options.flags), LP_EXPECT_HOSTNAME },

  /* LogReaderOptions */
  { "kernel",                     CFH_SET, offsetof(LogReaderOptions, flags),               LR_KERNEL },
  { "empty-lines",                CFH_SET, offsetof(LogReaderOptions, flags),               LR_EMPTY_LINES },
#if ENABLE_THREADED
  { "threaded",                   CFH_SET, offsetof(LogReaderOptions, flags),               LR_THREADED },
#else
  { "threaded",                   CFH_CLEAR, offsetof(LogReaderOptions, flags),               LR_THREADED },
#endif
  { NULL },
};

gboolean
log_reader_options_process_flag(LogReaderOptions *options, gchar *flag)
{
  return cfg_process_flag(log_reader_flag_handlers, options, flag);
}

LogProto *log_reader_get_proto(LogReader *self)
{
  return self->proto;
}
