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
