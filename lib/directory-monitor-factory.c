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

#include "directory-monitor-factory.h"
#include "directory-monitor-poll.h"

#if SYSLOG_NG_HAVE_INOTIFY
#include "directory-monitor-inotify.h"
#endif

#include <string.h>
#include <stdio.h>

MonitorMethod
directory_monitor_factory_get_monitor_method(const gchar *method)
{
  if (strcmp(method, "auto") == 0)
    {
      return MM_AUTO;
    }
  else if (strcmp(method, "poll") == 0)
    {
      return MM_POLL;
    }
#if SYSLOG_NG_HAVE_INOTIFY
  else if (strcmp(method, "inotify") == 0)
    {
      return MM_INOTIFY;
    }
#endif
  return MM_UNKNOWN;
}

DirectoryMonitorConstructor
directory_monitor_factory_get_constructor(DirectoryMonitorOptions *options)
{
  DirectoryMonitorConstructor constructor = NULL;
#if SYSLOG_NG_HAVE_INOTIFY
  if (options->method == MM_AUTO || options->method == MM_INOTIFY)
    {
      constructor = directory_monitor_inotify_new;
    }
  else if (options->method == MM_POLL)
    {
      constructor = directory_monitor_poll_new;
    }
#else
  if (options->method == MM_AUTO || options->method == MM_POLL)
    {
      constructor = directory_monitor_poll_new;
    }
#endif
  return constructor;
}

DirectoryMonitor *
create_directory_monitor(DirectoryMonitorOptions *options)
{
  DirectoryMonitor *monitor = NULL;
  DirectoryMonitorConstructor constructor = directory_monitor_factory_get_constructor(options);
  if (constructor)
    {
      monitor = constructor(options->dir, options->follow_freq);
    }
  return monitor;
}
