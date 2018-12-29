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

#ifndef MODULES_AFFILE_DIRECTORY_MONITOR_FACTORY_H_
#define MODULES_AFFILE_DIRECTORY_MONITOR_FACTORY_H_

#include "directory-monitor.h"

typedef DirectoryMonitor *(*DirectoryMonitorConstructor)(const gchar *dir, guint recheck_time);

typedef enum
{
  MM_AUTO,
  MM_POLL,
  MM_INOTIFY,
  MM_UNKNOWN
} MonitorMethod;

typedef struct _DirectoryMonitorOptions
{
  const gchar *dir;
  guint follow_freq;
  MonitorMethod method;
} DirectoryMonitorOptions;

DirectoryMonitorConstructor directory_monitor_factory_get_constructor(DirectoryMonitorOptions *options);
MonitorMethod directory_monitor_factory_get_monitor_method(const gchar *method_name);

DirectoryMonitor *create_directory_monitor(DirectoryMonitorOptions *options);

#endif /* MODULES_AFFILE_DIRECTORY_MONITOR_FACTORY_H_ */
