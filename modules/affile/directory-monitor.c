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

#include "directory-monitor.h"
#include "timeutils/misc.h"
#include "mainloop-call.h"
#include "pathutils.h"

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <messages.h>
#include <iv.h>

void
directory_monitor_stop(DirectoryMonitor *self)
{
  msg_debug("Stopping directory monitor", evt_tag_str("dir", self->dir));

  if (FALSE == main_loop_is_main_thread())
    {
      main_loop_call((MainLoopTaskFunc) directory_monitor_stop, self, TRUE);
      return;
    }

  if (iv_timer_registered(&self->check_timer))
    {
      iv_timer_unregister(&self->check_timer);
    }
  if (iv_task_registered(&self->scheduled_destructor))
    {
      iv_task_unregister(&self->scheduled_destructor);
    }
  if (self->stop_watches && self->watches_running)
    {
      self->stop_watches(self);
    }
  self->watches_running = FALSE;
}

static void
_collect_all_files(DirectoryMonitor *self, GDir *directory)
{
  const gchar *filename = g_dir_read_name(directory);
  while (filename)
    {
      DirectoryMonitorEvent event = { .name = filename };
      event.full_path = build_filename(self->full_path, filename);
      event.event_type = g_file_test(event.full_path, G_FILE_TEST_IS_DIR) ? DIRECTORY_CREATED : FILE_CREATED;
      self->callback(&event, self->callback_data);
      g_free(event.full_path);
      filename = g_dir_read_name(directory);
    }
}

void
rearm_timer(struct iv_timer *rescan_timer, gint rearm_time)
{
  iv_validate_now();
  rescan_timer->expires = iv_now;
  timespec_add_msec(&rescan_timer->expires, rearm_time);
  iv_timer_register(rescan_timer);
}

void
directory_monitor_start(DirectoryMonitor *self)
{
  main_loop_assert_main_thread();
  if (self->watches_running)
    return;

  msg_debug("Starting directory monitor", evt_tag_str("dir", self->full_path), evt_tag_str("dir_monitor_method",
            self->method));
  GError *error = NULL;
  GDir *directory = g_dir_open(self->full_path, 0, &error);
  if (!directory)
    {
      msg_error("Can not open directory",
                evt_tag_str("base_dir", self->full_path),
                evt_tag_str("error", error->message));
      rearm_timer(&self->check_timer, self->recheck_time);
      g_error_free(error);
      return;
    }
  _collect_all_files(self, directory);
  g_dir_close(directory);
  if (self->start_watches)
    {
      self->start_watches(self);
    }
  self->watches_running = TRUE;
  return;
}

void
directory_monitor_set_callback(DirectoryMonitor *self, DirectoryMonitorEventCallback callback, gpointer user_data)
{
  self->callback = callback;
  self->callback_data = user_data;
}

void
directory_monitor_schedule_destroy(DirectoryMonitor *self)
{
  if (!iv_task_registered(&self->scheduled_destructor))
    {
      iv_task_register(&self->scheduled_destructor);
    }
}

void
directory_monitor_stop_and_destroy(DirectoryMonitor *self)
{
  if (FALSE == main_loop_is_main_thread())
    {
      main_loop_call((MainLoopTaskFunc) directory_monitor_stop_and_destroy, self, TRUE);
      return;
    }

  directory_monitor_stop(self);
  directory_monitor_free(self);
}

gboolean directory_monitor_can_notify_file_changes(DirectoryMonitor *self)
{
  return self->can_notify_file_changes;
}

void
directory_monitor_init_instance(DirectoryMonitor *self, const gchar *dir, guint recheck_time, const gchar *method)
{
  self->method = method;
  self->dir = g_strdup(dir);
  self->full_path = canonicalize_filename(self->dir);
  self->can_notify_file_changes = FALSE;
  self->recheck_time = recheck_time;

  IV_TIMER_INIT(&self->check_timer);
  self->check_timer.handler = (GDestroyNotify) directory_monitor_start;
  self->check_timer.cookie = self;

  IV_TASK_INIT(&self->scheduled_destructor);
  self->scheduled_destructor.cookie = self;
  self->scheduled_destructor.handler = (GDestroyNotify)directory_monitor_stop_and_destroy;
}

DirectoryMonitor *
directory_monitor_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitor *self = g_new0(DirectoryMonitor, 1);
  directory_monitor_init_instance(self, dir, recheck_time, "unknown");
  return self;
}

void
directory_monitor_free(DirectoryMonitor *self)
{
  if (self)
    {
      msg_debug("Free directory monitor",
                evt_tag_str("dir", self->dir));
      if (self->free_fn)
        {
          self->free_fn(self);
        }
      g_free(self->full_path);
      g_free(self->dir);
      g_free(self);
    }
}
