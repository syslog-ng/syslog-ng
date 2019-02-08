/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "poll-file-changes.h"
#include "logpipe.h"
#include "timeutils/misc.h"

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


typedef struct _PollFileChanges
{
  PollEvents super;
  gint fd;
  gchar *follow_filename;
  gint follow_freq;
  struct iv_timer follow_timer;
  LogPipe *control;
} PollFileChanges;

/* follow timer callback. Check if the file has new content, or deleted or
 * moved.  Ran every follow_freq seconds.  */
static void
poll_file_changes_check_file(gpointer s)
{
  PollFileChanges *self = (PollFileChanges *) s;
  struct stat st, followed_st;
  off_t pos = -1;
  gint fd = self->fd;

  msg_trace("Checking if the followed file has new lines",
            evt_tag_str("follow_filename", self->follow_filename));
  if (fd >= 0)
    {
      pos = lseek(fd, 0, SEEK_CUR);
      if (pos == (off_t) -1)
        {
          msg_error("Error invoking seek on followed file",
                    evt_tag_error("error"));
          goto reschedule;
        }

      if (fstat(fd, &st) < 0)
        {
          if (errno == ESTALE)
            {
              msg_trace("log_reader_fd_check file moved ESTALE",
                        evt_tag_str("follow_filename", self->follow_filename));
              log_pipe_notify(self->control, NC_FILE_MOVED, self);
              return;
            }
          else
            {
              msg_error("Error invoking fstat() on followed file",
                        evt_tag_error("error"));
              goto reschedule;
            }
        }

      msg_trace("log_reader_fd_check",
                evt_tag_int("pos", pos),
                evt_tag_int("size", st.st_size));

      if (pos < st.st_size || !S_ISREG(st.st_mode))
        {
          /* we have data to read */
          poll_events_invoke_callback(s);
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
                        evt_tag_str("follow_filename", self->follow_filename));
              /* file was moved and we are at EOF, follow the new file */
              log_pipe_notify(self->control, NC_FILE_MOVED, self);
              /* we may be freed by the time the notification above returns */
              return;
            }
        }
      else
        {
          msg_verbose("Follow mode file still does not exist",
                      evt_tag_str("filename", self->follow_filename));
        }
    }
reschedule:
  poll_events_update_watches(s, G_IO_IN);
}

static void
poll_file_changes_stop_watches(PollEvents *s)
{
  PollFileChanges *self = (PollFileChanges *) s;

  if (iv_timer_registered(&self->follow_timer))
    iv_timer_unregister(&self->follow_timer);
}

static void
poll_file_changes_rearm_timer(PollFileChanges *self)
{
  iv_validate_now();
  self->follow_timer.expires = iv_now;
  timespec_add_msec(&self->follow_timer.expires, self->follow_freq);
  iv_timer_register(&self->follow_timer);
}

static void
poll_file_changes_update_watches(PollEvents *s, GIOCondition cond)
{
  PollFileChanges *self = (PollFileChanges *) s;

  /* we can only provide input events */
  g_assert((cond & ~G_IO_IN) == 0);

  poll_file_changes_stop_watches(s);

  if (cond & G_IO_IN)
    poll_file_changes_rearm_timer(self);
}

static void
poll_file_changes_free(PollEvents *s)
{
  PollFileChanges *self = (PollFileChanges *) s;

  log_pipe_unref(self->control);
  g_free(self->follow_filename);
}

PollEvents *
poll_file_changes_new(gint fd, const gchar *follow_filename, gint follow_freq, LogPipe *control)
{
  PollFileChanges *self = g_new0(PollFileChanges, 1);

  self->super.stop_watches = poll_file_changes_stop_watches;
  self->super.update_watches = poll_file_changes_update_watches;
  self->super.free_fn = poll_file_changes_free;

  self->fd = fd;
  self->follow_filename = g_strdup(follow_filename);
  self->follow_freq = follow_freq;
  self->control = log_pipe_ref(control);

  IV_TIMER_INIT(&self->follow_timer);
  self->follow_timer.cookie = self;
  self->follow_timer.handler = poll_file_changes_check_file;

  return &self->super;
}
