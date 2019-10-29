/*
 * Copyright (c) 2019 One Identity
 * Copyright (c) 2019 László Várady
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

#include "syslog-ng.h"
#include "poll-multiline-file-changes.h"
#include "messages.h"

#include <iv.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct PollMultilineFileChanges
{
  PollFileChanges super;
  FileReader *file_reader;
  gint multi_line_timeout;

  gboolean timed_out;
  gint64 last_eof;
} PollMultilineFileChanges;


static void
_flush_partial_message(PollMultilineFileChanges *self)
{
  file_reader_cue_buffer_flush(self->file_reader);

  /* invoke fetch */
  poll_events_invoke_callback(&self->super.super);
}

static void
poll_multiline_file_changes_start_timer(PollMultilineFileChanges *self)
{
  self->last_eof = g_get_monotonic_time();
}

static void
poll_multiline_file_changes_stop_timer(PollMultilineFileChanges *self)
{
  self->last_eof = 0;
}

static void
poll_multiline_file_changes_timeout_expired(PollMultilineFileChanges *self)
{
  msg_debug("Multi-line timeout has elapsed, processing partial message",
            evt_tag_str("filename", self->super.follow_filename));

  self->last_eof = 0;
  self->timed_out = TRUE;
  _flush_partial_message(self);
}

static gboolean
_is_multi_line_timeout_pending(PollMultilineFileChanges *self)
{
  return !self->timed_out && self->last_eof != 0;
}

static gboolean
_is_multi_line_timeout_expired(PollMultilineFileChanges *self)
{
  if (self->last_eof == 0)
    return FALSE;

  gint64 millisecs_since_last_eof = (g_get_monotonic_time() - self->last_eof) / 1000;
  return millisecs_since_last_eof > self->multi_line_timeout;
}

static void
poll_multiline_file_changes_on_eof(PollFileChanges *s)
{
  PollMultilineFileChanges *self = (PollMultilineFileChanges *) s;

  if (_is_multi_line_timeout_pending(self))
    {
      poll_multiline_file_changes_start_timer(self);
      return;
    }

  if (_is_multi_line_timeout_expired(self))
    poll_multiline_file_changes_timeout_expired(self);
}

static void
poll_multiline_file_changes_on_file_moved(PollFileChanges *s)
{
  PollMultilineFileChanges *self = (PollMultilineFileChanges *) s;

  poll_multiline_file_changes_stop_timer(self);
}

static void
poll_multiline_file_changes_on_read(PollFileChanges *s)
{
  PollMultilineFileChanges *self = (PollMultilineFileChanges *) s;

  poll_multiline_file_changes_stop_timer(self);
  self->timed_out = FALSE;
}

static void
poll_multiline_file_changes_update_watches(PollEvents *s, GIOCondition cond)
{
  PollMultilineFileChanges *self = (PollMultilineFileChanges *) s;

  if (!cond)
    poll_multiline_file_changes_stop_timer(self);

  poll_file_changes_update_watches(s, cond);
}

static void
poll_multiline_file_changes_stop_watches(PollEvents *s)
{
  PollMultilineFileChanges *self = (PollMultilineFileChanges *) s;

  poll_multiline_file_changes_stop_timer(self);
  poll_file_changes_stop_watches(s);
}

PollEvents *
poll_multiline_file_changes_new(gint fd, const gchar *follow_filename, gint follow_freq,
                                gint multi_line_timeout, FileReader *reader)
{
  PollMultilineFileChanges *self = g_new0(PollMultilineFileChanges, 1);
  poll_file_changes_init_instance(&self->super, fd, follow_filename, follow_freq, &reader->super);

  self->multi_line_timeout = multi_line_timeout;

  if (!self->multi_line_timeout)
    return &self->super.super;

  self->file_reader = reader;
  self->super.on_read = poll_multiline_file_changes_on_read;
  self->super.on_eof = poll_multiline_file_changes_on_eof;
  self->super.on_file_moved = poll_multiline_file_changes_on_file_moved;

  self->super.super.update_watches = poll_multiline_file_changes_update_watches;
  self->super.super.stop_watches = poll_multiline_file_changes_stop_watches;

  return &self->super.super;
}
