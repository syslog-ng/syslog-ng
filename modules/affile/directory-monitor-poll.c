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

#include <collection-comparator.h>
#include "directory-monitor-poll.h"
#include "timeutils.h"

#include <iv.h>

typedef struct _DirectoryMonitorPoll
{
  DirectoryMonitor super;
  CollectionComparator *comparator;
  struct iv_timer rescan_timer;
} DirectoryMonitorPoll;

static void
_handle_new_entry(const gchar *filename, gpointer user_data)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)user_data;
  DirectoryMonitorEvent event;

  event.name = filename;
  event.full_path = build_filename(self->super.real_path, event.name);
  event.event_type = g_file_test(event.full_path, G_FILE_TEST_IS_DIR) ? DIRECTORY_CREATED : FILE_CREATED;
  if (self->super.callback)
    {
      self->super.callback(&event, self->super.callback_data);
    }
  g_free(event.full_path);
}

static void
_handle_deleted_entry(const gchar *filename, gpointer user_data)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)user_data;
  DirectoryMonitorEvent event;

  event.name = filename;
  event.event_type = FILE_DELETED;
  event.full_path = build_filename(self->super.real_path, event.name);
  if (self->super.callback)
    {
      self->super.callback(&event, self->super.callback_data);
    }
  g_free(event.full_path);
}

static void
_handle_deleted_self(DirectoryMonitorPoll *self)
{
  DirectoryMonitorEvent event;

  event.name = self->super.real_path;
  event.event_type = DIRECTORY_DELETED;
  event.full_path = self->super.real_path;
  if (self->super.callback)
    {
      self->super.callback(&event, self->super.callback_data);
    }
}

static void
_rescan_directory(DirectoryMonitorPoll *self)
{
  GError *error = NULL;
  GDir *directory = g_dir_open(self->super.real_path, 0, &error);
  collection_comparator_start(self->comparator);
  if (directory)
    {
      const gchar *filename;
      while((filename = g_dir_read_name(directory)))
        {
          collection_comparator_add_value(self->comparator, filename);
        }
      g_dir_close(directory);
      collection_comparator_stop(self->comparator);
    }
  else
    {
      collection_comparator_stop(self->comparator);
      _handle_deleted_self(self);
      g_clear_error(&error);
    }
}

void
_rearm_rescan_timer(DirectoryMonitorPoll *self)
{
  iv_validate_now();
  self->rescan_timer.expires = iv_now;
  timespec_add_msec(&self->rescan_timer.expires, self->super.recheck_time);
  iv_timer_register(&self->rescan_timer);
}

static void
_start_watches(DirectoryMonitor *s)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)s;
  GDir *directory = NULL;
  GError *error = NULL;
  directory = g_dir_open(self->super.real_path, 0, &error);
  if (directory)
    {
      const gchar *filename = g_dir_read_name(directory);
      while (filename)
        {
          collection_comparator_add_initial_value(self->comparator, filename);
          filename = g_dir_read_name(directory);
        }
      g_dir_close(directory);
    }
  _rearm_rescan_timer(self);
}


static void
_stop_watches(DirectoryMonitor *s)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)s;
  if (iv_timer_registered(&self->rescan_timer))
    {
      iv_timer_unregister(&self->rescan_timer);
    }
}

static void
_free_fn(DirectoryMonitor *s)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)s;
  collection_comparator_free(self->comparator);
}

static void
_triggered_timer(gpointer data)
{
  DirectoryMonitorPoll *self = (DirectoryMonitorPoll *)data;
  _rescan_directory(self);
  _rearm_rescan_timer(self);
}

DirectoryMonitor *
directory_monitor_poll_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitorPoll *self = g_new0(DirectoryMonitorPoll, 1);
  directory_monitor_init_instance(&self->super, dir, recheck_time);

  IV_TIMER_INIT(&self->rescan_timer);
  self->rescan_timer.cookie = self;
  self->rescan_timer.handler = _triggered_timer;
  self->super.start_watches = _start_watches;
  self->super.stop_watches = _stop_watches;
  self->super.free_fn = _free_fn;
  self->comparator = collection_comparator_new();
  collection_comporator_set_callbacks(self->comparator,
                                      _handle_new_entry,
                                      _handle_deleted_entry,
                                      self);
  return &self->super;
}
