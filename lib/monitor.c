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

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>

#if SYSLOG_NG_HAVE_INOTIFY
#include <iv_inotify.h>

struct iv_inotify_watch *w;
struct iv_inotify inotify;

MainLoop *_self;
const gchar *_file_path;

static void
handler(void *_dummy, struct inotify_event *event)
{

  printf("MASK = %x\n", event->mask);

  // handle case where editing the file using vim causes an event 'IN_IGNORED'
  // and subsequently the watch gets removed by ivykis: ivykis/src/iv_inotify.c
  if (event->mask & IN_MODIFY)
    main_loop_reload_config(_self);

  else if (event->mask & IN_IGNORED)
  {
   iv_inotify_unregister(&inotify);
   iv_inotify_watch_unregister(w);
   free(w);
   initialize_inotify();
   main_loop_reload_config(_self);
  }
}

void 
initialize_inotify()
{
  IV_INOTIFY_INIT(&inotify);
  iv_inotify_register(&inotify);
  w = malloc(sizeof(*w));

  if (w == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  IV_INOTIFY_WATCH_INIT(w);
  w->inotify = &inotify;
  w->mask = IN_MODIFY|IN_IGNORED;
  w->handler = handler;
  w->pathname = _file_path;
  w->cookie = w;
  iv_inotify_watch_register(w);
}

void
monitor_start(const gchar *file_path, MainLoop *self)
{
  _self = self;
  _file_path = file_path; 
  initialize_inotify();
  iv_main();
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

#endif

