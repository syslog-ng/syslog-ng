/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2024 OneIdentity
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
#include "directory-monitor-kqueue.h"
#include "directory-monitor-content-comparator.h"
#include "messages.h"

#include <iv.h>
#include <iv_event.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct _DirectoryMonitorKQueue
{
  DirectoryMonitorContentComparator super;
  struct iv_fd kqueue_fd;
  gint dir_fd;
} DirectoryMonitorKQueue;

static void handle_kqueue_event(void *s)
{
  DirectoryMonitorKQueue *self = (DirectoryMonitorKQueue *)s;

  struct kevent events[64];
  struct timespec timeout = {0, 0}; // non-blocking

  while (1)
    {
      int nevents = kevent(self->kqueue_fd.fd, NULL, 0, events,
                           sizeof(events) / sizeof(events[0]), &timeout);
      if (nevents < 0)
        msg_error("directory-monitor-kqueue: kevent returned with error",
                  evt_tag_error("errno"));
      else if (nevents == 0)
        break;
    }

  /* NOTE: kevent has no exact info about the directory changes like e.g ivykis has, so
   *       using a combo of event based watching and (poll) changes comparator here
   */
  directory_monitor_content_comparator_rescan_directory(&self->super, FALSE);
}

static gboolean
_register_kqueue_watches(DirectoryMonitorKQueue *self)
{
#ifdef __APPLE__
  gint open_mode = O_EVTONLY | O_DIRECTORY;
#else
  gint open_mode = O_NONBLOCK | O_RDONLY | O_DIRECTORY;
#endif
  gint fd = open(self->super.super.full_path, open_mode);
  if (fd < 0)
    {
      msg_error("directory-monitor-kqueue: could not open directory for watching",
                evt_tag_str("dir", self->super.super.full_path),
                evt_tag_error("errno"));
      return FALSE;
    }

  int kq = kqueue();
  if (kq < 0)
    {
      close(fd);
      msg_error("directory-monitor-kqueue: could not create kqueue object",
                evt_tag_error("errno"));
      return FALSE;
    }

  struct kevent change;
  EV_SET(&change, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
         NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_REVOKE, 0, NULL);

  if (kevent(kq, &change, 1, NULL, 0, NULL) < 0)
    {
      close(fd);
      close(kq);
      msg_error("directory-monitor-kqueue: could not reqister kevent watcher",
                evt_tag_error("errno"));
      return FALSE;
    }

  self->dir_fd = fd;

  IV_FD_INIT(&self->kqueue_fd);
  self->kqueue_fd.fd = kq;
  self->kqueue_fd.cookie = self;
  self->kqueue_fd.handler_in = handle_kqueue_event;

  return TRUE;
}

static void
_unregister_kqueue_watches(DirectoryMonitorKQueue *self)
{
  close(self->kqueue_fd.fd);
  close(self->dir_fd);
}

static void
_start_watches(DirectoryMonitor *s)
{
  DirectoryMonitorKQueue *self = (DirectoryMonitorKQueue *)s;

  if (_register_kqueue_watches(self))
    {
      msg_trace("Starting to watch directory changes", evt_tag_str("dir", self->super.super.full_path));
      directory_monitor_content_comparator_rescan_directory(&self->super, TRUE);

      iv_fd_register(&self->kqueue_fd);
    }
}

static void
_stop_watches(DirectoryMonitor *s)
{
  DirectoryMonitorKQueue *self = (DirectoryMonitorKQueue *)s;

  if (iv_fd_registered(&self->kqueue_fd))
    {
      iv_fd_unregister(&self->kqueue_fd);
      _unregister_kqueue_watches(self);
    }
}

static void
_free_fn(DirectoryMonitor *s)
{
  DirectoryMonitorKQueue *self = (DirectoryMonitorKQueue *)s;

  directory_monitor_content_comparator_free(&self->super);
}

DirectoryMonitor *
directory_monitor_kqueue_new(const gchar *dir, guint recheck_time)
{
  DirectoryMonitorKQueue *self = g_new0(DirectoryMonitorKQueue, 1);

  directory_monitor_content_comparator_init_instance(&self->super, dir, recheck_time, "kqueue");

  self->super.super.free_fn = _free_fn;
  self->super.super.start_watches = _start_watches;
  self->super.super.stop_watches = _stop_watches;

  return &self->super.super;
}
