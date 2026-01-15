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

#include "directory-monitor-factory.h"
#include "directory-monitor-poll.h"

/* NOTE: If both kqueue and inotify are present, like on FreeBSD, where the inotify wrapper started to be distributed,
 *       we prioritize kqueue in the support detection in the cmake/autotools configure files, as inotify is just
 *       a wrapper around kqueue if both are available.
 *       Thus at this point only one of the
 *            - SYSLOG_NG_HAVE_INOTIFY or SYSLOG_NG_HAVE_KQUEUE will be defined
 *            - directory-monitor-inotify.h or directory-monitor-kqueue.h will be included
 *            - directory-monitor-inotify.c or directory-monitor-kqueue.c will be built and linked
 */

#if SYSLOG_NG_HAVE_INOTIFY
#include "directory-monitor-inotify.h"
#elif SYSLOG_NG_HAVE_KQUEUE
#include "directory-monitor-kqueue.h"
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
#if SYSLOG_NG_HAVE_KQUEUE
  else if (strcmp(method, "kqueue") == 0)
    {
      return MM_KQUEUE;
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
#endif

#if SYSLOG_NG_HAVE_KQUEUE
  if (constructor == NULL && (options->method == MM_AUTO || options->method == MM_KQUEUE))
    {
      constructor = directory_monitor_kqueue_new;
    }
#endif

  if (constructor == NULL && (options->method == MM_AUTO || options->method == MM_POLL))
    {
      constructor = directory_monitor_poll_new;
    }

  return constructor;
}

DirectoryMonitor *
create_directory_monitor(DirectoryMonitorOptions *options)
{
  DirectoryMonitor *monitor = NULL;
  DirectoryMonitorConstructor constructor = directory_monitor_factory_get_constructor(options);
  if (constructor)
    {
      monitor = constructor(options->dir, options->monitor_freq);
    }
  return monitor;
}
