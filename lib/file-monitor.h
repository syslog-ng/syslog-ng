/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023 László Várady
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

#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

#include "syslog-ng.h"
#include <sys/stat.h>

typedef struct _FileMonitor FileMonitor;

typedef struct _FileMonitorEvent
{
  const gchar *name;
  struct stat st;
  enum
  {
    MODIFIED,
    DELETED
  } event;
} FileMonitorEvent;

typedef gboolean (*FileMonitorEventCB)(const FileMonitorEvent *event, gpointer c);

FileMonitor *file_monitor_new(const gchar *file_name);
void file_monitor_free(FileMonitor *self);

void file_monitor_add_watch(FileMonitor *self, FileMonitorEventCB cb,  gpointer cb_data);
void file_monitor_remove_watch(FileMonitor *self, FileMonitorEventCB cb, gpointer cb_data);

void file_monitor_start_and_check(FileMonitor *self);
void file_monitor_start(FileMonitor *self);
void file_monitor_stop(FileMonitor *self);

#endif
