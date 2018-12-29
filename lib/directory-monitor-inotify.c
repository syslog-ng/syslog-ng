/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "directory-monitor-inotify.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct _DirectoryMonitorInotify
{
  DirectoryMonitor super;
  struct iv_inotify inotify;
  struct iv_inotify_watch watcher;
} DirectoryMonitorInotify;


static DirectoryMonitorEventType
_get_event_type(struct inotify_event *event, gchar *filename)
{
  if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO))
    {
      if (event->mask & IN_ISDIR)
        {
          return DIRECTORY_CREATED;
        }
      return FILE_CREATED;
    }
  else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM))
    {
      return FILE_DELETED;
    }
  else if ((event->mask & IN_DELETE_SELF) || (event->mask & IN_MOVE_SELF))
    {
      return DIRECTORY_DELETED;
    }
  else if (event->mask & IN_MODIFY)
    {
      return FILE_MODIFIED;
    }
  return UNKNOWN;
}

static void
_handle_event(gpointer s, struct inotify_event *event)
{
  DirectoryMonitorInotify *self = (DirectoryMonitorInotify *)s;
  DirectoryMonitorEvent dir_event;
  dir_event.name = g_strdup_printf("%.*s", event->len, &event->name[0]);
  dir_event.full_path = build_filename(self->super.real_path, dir_event.name);
  dir_event.event_type = _get_event_type(event, dir_event.full_path);
  if (self->super.callback && dir_event.event_type != UNKNOWN)
    {
      self->super.callback(&dir_event, self->super.callback_data);
    }
  g_free(dir_event.full_path);
  g_free((gchar *)dir_event.name);
}

static void
_start_watches(DirectoryMonitor *s)
{
  DirectoryMonitorInotify *self = (DirectoryMonitorInotify *)s;

  IV_INOTIFY_WATCH_INIT(&self->watcher);
  self->watcher.inotify = &self->inotify;
  self->watcher.pathname = self->super.dir;
  self->watcher.mask = IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MODIFY;
  self->watcher.cookie = self;
  self->watcher.handler = _handle_event;
  iv_inotify_watch_register(&self->watcher);
}

static void
_stop_watches(DirectoryMonitor *s)
{
  DirectoryMonitorInotify *self = (DirectoryMonitorInotify *)s;
  iv_inotify_watch_unregister(&self->watcher);
}

static void
_free(DirectoryMonitor *s)
{
  DirectoryMonitorInotify *self = (DirectoryMonitorInotify *)s;
  iv_inotify_unregister(&self->inotify);
}

DirectoryMonitor *
directory_monitor_inotify_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitorInotify *self = g_new0(DirectoryMonitorInotify, 1);
  directory_monitor_init_instance(&self->super, dir, recheck_time);

  IV_INOTIFY_INIT(&self->inotify);
  if (iv_inotify_register(&self->inotify))
    {
      msg_error("directory-monitor-inotify: could not create inotify object", evt_tag_error("errno"));
      directory_monitor_free(&self->super);
      return NULL;
    }

  self->super.start_watches = _start_watches;
  self->super.stop_watches = _stop_watches;
  self->super.free_fn = _free;

  return &self->super;
}
