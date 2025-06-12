/*
 * Copyright (c) 2017 Balabit
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

#include "directory-monitor-poll.h"
#include "directory-monitor-content-comparator.h"
#include "messages.h"
#include "timeutils/misc.h"
#include "glib.h"
#include "glib/gstdio.h"

#include <iv.h>

typedef struct _DirectoryMonitorPoll
{
  DirectoryMonitorContentComparator super;
  struct iv_timer rescan_timer;
} DirectoryMonitorPoll;


static void
_start_watches(DirectoryMonitor *s)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)s;
  msg_trace("Starting to poll directory changes",
            evt_tag_str("dir", self->super.super.full_path),
            evt_tag_int("freq", self->super.super.recheck_time));
  directory_monitor_content_comparator_rescan_directory(&self->super, TRUE);
  rearm_timer(&self->rescan_timer, self->super.super.recheck_time);
}


static void
_stop_watches(DirectoryMonitor *s)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)s;
  if (iv_timer_registered(&self->rescan_timer))
    iv_timer_unregister(&self->rescan_timer);
}

static void
_triggered_timer(gpointer data)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)data;
  directory_monitor_content_comparator_rescan_directory(&self->super, FALSE);
  rearm_timer(&self->rescan_timer, self->super.super.recheck_time);
}

DirectoryMonitor *
directory_monitor_poll_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitorPoll *self = g_new0(DirectoryMonitorPoll, 1);

  directory_monitor_content_comparator_init_instance(&self->super, dir, recheck_time, "poll");

  IV_TIMER_INIT(&self->rescan_timer);
  self->rescan_timer.cookie = self;
  self->rescan_timer.handler = _triggered_timer;
  self->super.super.start_watches = _start_watches;
  self->super.super.stop_watches = _stop_watches;

  return &self->super.super;
}
