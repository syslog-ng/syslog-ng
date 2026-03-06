/*
 * Copyright (c) 2025 One Identity
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

#include "file-monitor-factory.h"
#include "notified-fd-events.h"
#include "poll-fd-events.h"
#include "poll-file-changes.h"
#include "poll-multiline-file-changes.h"

#include <string.h>
#include <stdio.h>

FollowMethod
file_monitor_factory_follow_method_from_string(const gchar *method)
{
  if (strcmp(method, "auto") == 0)
    {
      return FM_AUTO;
    }
  else if (strcmp(method, "legacy") == 0)
    {
      return FM_LEGACY;
    }
  else if (strcmp(method, "poll") == 0)
    {
      return FM_POLL;
    }
  else if (strcmp(method, "system") == 0)
    {
      return FM_SYSTEM_POLL;
    }
#if SYSLOG_NG_HAVE_INOTIFY
  else if (strcmp(method, "inotify") == 0)
    {
      return FM_INOTIFY;
    }
#endif
  return FM_UNKNOWN;
}

PollEvents *
create_file_monitor_events(FileReader *self, FollowMethod file_follow_mode, gint fd)
{
  PollEvents *poll_events = NULL;

  switch (file_follow_mode)
    {
    case FM_SYSTEM_POLL:
      poll_events = poll_fd_events_new(fd);
      break;

#if SYSLOG_NG_HAVE_INOTIFY
    case FM_INOTIFY:
      poll_events = notified_fd_events_new(fd);
      break;
#endif

    case FM_POLL:
      if (file_reader_options_get_log_proto_options(self->options)->super.super.multi_line_options.mode == MLM_NONE)
        poll_events = poll_file_changes_new(fd, self->filename->str, self->options->follow_freq, self);
      else
        poll_events = poll_multiline_file_changes_new(fd, self->filename->str, self->options->follow_freq,
                                                      self->options->multi_line_timeout, self);
      break;

    default:
      g_assert_not_reached();
      break;
    }
  return poll_events;
}
