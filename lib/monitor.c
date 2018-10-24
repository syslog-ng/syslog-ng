/*
 * Copyright (c) 2002-2018 Balabit
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

#include "monitor.h"

//#if SYSLOG_NG_HAVE_INOTIFY
static void
handler(void *_fm, struct inotify_event *event)
{
  FileMonitor *fm = (FileMonitor *) _fm;

  if (event->mask & IN_MODIFY)
    fm->main_loop_reload_config(fm->self);

  /* 1- handle case where editing the file using vim causes an event 'IN_IGNORED'
       and subsequently the watch gets deleted by ivykis: ivykis/src/iv_inotify.c:88
     2- handle case when file is edited and contains a parse error, syslog-ng changes
       file attributes and file becomes unreadable

      - note : for case when file was deleted, syslog-ng would still attempt to reload,
             which gives a helpful "file not found" error in logs -> but auto-reload will then be disabled
             since there's no file to watch anymore. ``syslog-ng-ctl reload`` should be issued after file is restored */

  else if (event->mask & IN_IGNORED || event->mask & IN_ATTRIB)
    {
      iv_inotify_unregister(&(fm->inotify));
      iv_inotify_watch_unregister(fm->w);
      file_monitor_start(fm);
      fm->main_loop_reload_config(fm->self);
    }
}

FileMonitor *
file_monitor_create(const gchar *file_path)
{
  FileMonitor *fm;
  struct iv_inotify_watch *w;

  fm = g_malloc0(sizeof(*fm));
  w = g_malloc0(sizeof(*w));

  if ((fm == NULL) | (w == NULL))
    {
      perror("malloc");
      exit(EXIT_FAILURE);
    }

  fm->w = w;
  fm->file_path = file_path;
  return fm;
}

void
file_monitor_config_reload_callback_function(FileMonitor *fm, void (*mainloop_callback)(void *), gpointer *self)
{
  fm->main_loop_reload_config = mainloop_callback;
  fm->w->cookie = (void *) fm;
  fm->self = self;
}

void
file_monitor_start(FileMonitor *fm)
{
  IV_INOTIFY_INIT(&(fm->inotify));
  iv_inotify_register(&(fm->inotify));

  IV_INOTIFY_WATCH_INIT(fm->w);
  fm->w->inotify = &(fm->inotify);
  fm->w->mask = IN_ALL_EVENTS;
  fm->w->pathname = fm->file_path;
  iv_inotify_watch_register(fm->w);
  fm->w->handler = handler;
}


//#else

/*
MainLoop *_self;
const gchar *_file_path;
gchar *original_contents;
GError err;

static void
handler(void *_dummy, struct inotify_event *event)
{
  if (!g_file_get_contents(file_path, &original_contents, NULL, &err))
    {
      printf("Cannot read configuration file: %s", err -> message );
      g_error_free(err);
      err = NULL;
      return;
    }
  // after timeout - compare - different?
  main_loop_reload_config(_self);
}

void
monitor_start(const gchar *file_path, MainLoop *self)
{
  _self = self;
  _file_path = file_path;
}
*/

//#endif

