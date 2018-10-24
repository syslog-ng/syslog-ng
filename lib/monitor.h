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
#ifndef MONITOR_H_INCLUDED
#define MONITOR_H_INCLUDED

#include "messages.h"

#include <stdio.h>
#include <stdlib.h>
#include <iv.h>

//#if SYSLOG_NG_HAVE_INOTIFY
#include <iv_inotify.h>

typedef struct _FileMonitor
{
  struct iv_inotify_watch *w;
  struct iv_inotify inotify;
  gpointer *self;
  const gchar *file_path;
  void (*main_loop_reload_config)(void *);
} FileMonitor;

//#else

//#endif

FileMonitor *file_monitor_create(const gchar *file_path);
void file_monitor_config_reload_callback_function(FileMonitor *fm, void (*mainloop_callback)(void *), gpointer *self);
void file_monitor_start(FileMonitor *fm);

#endif
