/*
 * Copyright (c) 2024-2025 Axoflow
 * Copyright (c) 2023-2025 László Várady
 * Copyright (c) 2024 shifter
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

#include "file-monitor.h"
#include "messages.h"
#include "timeutils/misc.h"

#include <iv.h>

#if SYSLOG_NG_HAVE_INOTIFY
#include <iv_inotify.h>
#endif

#define FILE_MONITOR_POLL_FREQ (15 * 60)

typedef struct _FileMonitorCallbackListItem
{
  FileMonitorEventCB cb;
  gpointer cb_data;
} FileMonitorCallbackListItem;

typedef GList FileMonitorCallbackList;

struct _FileMonitor
{
  FileMonitorCallbackList *callbacks;

  const gchar *file_name;
  struct iv_timer poll_timer;
  time_t last_mtime;

#if SYSLOG_NG_HAVE_INOTIFY
  gboolean in_started;
  const gchar *file_basename;
  struct iv_inotify inotify;
  struct iv_inotify_watch in_watch;
#endif
};

void
file_monitor_add_watch(FileMonitor *self, FileMonitorEventCB cb,  gpointer cb_data)
{
  if (!cb)
    return;

  FileMonitorCallbackListItem *item = g_new(FileMonitorCallbackListItem, 1);
  item->cb = cb;
  item->cb_data = cb_data;

  self->callbacks = g_list_prepend(self->callbacks, item);

}

static gint
_compare_callback(gconstpointer a, gconstpointer b)
{
  const FileMonitorCallbackListItem *item_a = (const FileMonitorCallbackListItem *) a;
  const FileMonitorCallbackListItem *item_b = (const FileMonitorCallbackListItem *) b;

  gboolean equal = item_a->cb == item_b->cb && item_a->cb_data == item_b->cb_data;
  return equal ? 0 : -1;
}

void
file_monitor_remove_watch(FileMonitor *self, FileMonitorEventCB cb, gpointer cb_data)
{
  FileMonitorCallbackListItem item = { cb, cb_data };
  GList *i = g_list_find_custom(self->callbacks, &item, _compare_callback);

  if (!i)
    return;

  FileMonitorCallbackListItem *removed_item = i->data;
  self->callbacks = g_list_delete_link(self->callbacks, i);
  g_free(removed_item);
}

static void
_run_callbacks(FileMonitor *self, const FileMonitorEvent *event)
{
  for (FileMonitorCallbackList *i = self->callbacks; i; i = i->next)
    {
      FileMonitorCallbackListItem *item = i->data;
      item->cb(event, item->cb_data);
    }
}

static void
_run_callbacks_if_file_was_modified(FileMonitor *self)
{
  struct stat st = {0};
  FileMonitorEvent event =
  {
    .name = self->file_name,
    .event = MODIFIED,
    .st = st,
  };

  if (stat(self->file_name, &st) == -1)
    {
      event.event = DELETED;
      goto callbacks;
    }

  event.st = st;

  if (self->last_mtime >= st.st_mtime)
    return;

callbacks:
  _run_callbacks(self, &event);
  self->last_mtime = st.st_mtime;
}

static void
_poll_start(FileMonitor *self)
{
  if (iv_timer_registered(&self->poll_timer))
    return;

  iv_validate_now();
  self->poll_timer.expires = iv_now;
  timespec_add_msec(&self->poll_timer.expires, FILE_MONITOR_POLL_FREQ * 1000);
  iv_timer_register(&self->poll_timer);
}

static void
_poll_stop(FileMonitor *self)
{
  if (iv_timer_registered(&self->poll_timer))
    iv_timer_unregister(&self->poll_timer);
}

static void
_poll_timer_tick(gpointer c)
{
  FileMonitor *self = (FileMonitor *) c;

  _run_callbacks_if_file_was_modified(self);

  _poll_start(self);
}

#if SYSLOG_NG_HAVE_INOTIFY

static void _inotify_event_handler(void *c, struct inotify_event *event)
{
  FileMonitor *self = (FileMonitor *) c;

  if (g_strcmp0(self->file_basename, event->name) == 0)
    _run_callbacks_if_file_was_modified(self);
}

static gboolean
_inotify_start(FileMonitor *self)
{
  if (self->in_started)
    return FALSE;

  IV_INOTIFY_INIT(&self->inotify);
  if (iv_inotify_register(&self->inotify) != 0)
    {
      msg_warning("Error creating file monitor instance, can not register inotify", evt_tag_error("errno"));
      return FALSE;
    }

  gchar *file_dir = g_path_get_dirname(self->file_name);

  IV_INOTIFY_WATCH_INIT(&self->in_watch);
  self->in_watch.inotify = &self->inotify;
  self->in_watch.pathname = file_dir;
  self->in_watch.mask = IN_CREATE | IN_MOVE | IN_CLOSE_WRITE | IN_DELETE;
  self->in_watch.handler = _inotify_event_handler;
  self->in_watch.cookie = self;

  if (iv_inotify_watch_register(&self->in_watch) != 0)
    {
      iv_inotify_unregister(&self->inotify);
      g_free(file_dir);
      msg_warning("Error start file monitor, can not register inotify watch", evt_tag_error("errno"));
      return FALSE;
    }

  g_free(file_dir);

  self->file_basename = g_path_get_basename(self->file_name);

  self->in_started = TRUE;
  return TRUE;
}

static void
_inotify_stop(FileMonitor *self)
{
  if (!self->in_started)
    return;

  iv_inotify_watch_unregister(&self->in_watch);
  iv_inotify_unregister(&self->inotify);

  g_free((gchar *) self->file_basename);

  self->in_started = FALSE;
}

#else
#define _inotify_start(self) (FALSE)
#define _inotify_stop(self)
#endif

void file_monitor_start(FileMonitor *self)
{
  if (!_inotify_start(self))
    _poll_start(self);
}

void file_monitor_start_and_check(FileMonitor *self)
{
  file_monitor_start(self);
  _run_callbacks_if_file_was_modified(self);
}

void file_monitor_stop(FileMonitor *self)
{
  _inotify_stop(self);
  _poll_stop(self);
}

void
file_monitor_free(FileMonitor *self)
{
  g_list_free_full(self->callbacks, g_free);
  g_free((gchar *)self->file_name);
  g_free(self);
}

FileMonitor *
file_monitor_new(const gchar *file_name)
{
  FileMonitor *self = g_new0(FileMonitor, 1);

  IV_TIMER_INIT(&self->poll_timer);
  self->poll_timer.handler = _poll_timer_tick;
  self->poll_timer.cookie = self;
  self->file_name = g_strdup(file_name);

  return self;
}
