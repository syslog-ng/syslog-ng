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

#ifndef MODULES_AFFILE_DIRECTORY_MONITOR_H_
#define MODULES_AFFILE_DIRECTORY_MONITOR_H_

#include <syslog-ng.h>
#include <iv.h>

typedef enum
{
  FILE_CREATED,
  DIRECTORY_CREATED,
  FILE_DELETED,
  DIRECTORY_DELETED,
  UNKNOWN
} DirectoryMonitorEventType;

typedef struct _DirectoryMonitorEvent
{
  const gchar *name;
  gchar *full_path;
  DirectoryMonitorEventType event_type;
} DirectoryMonitorEvent;

typedef  void (*DirectoryMonitorEventCallback)(const DirectoryMonitorEvent *event,
                                               gpointer user_data);

typedef struct _DirectoryMonitor DirectoryMonitor;

struct _DirectoryMonitor
{
  gchar *dir;
  gchar *real_path;
  DirectoryMonitorEventCallback callback;
  gpointer callback_data;

  guint recheck_time;
  struct iv_timer check_timer;
  struct iv_task scheduled_destructor;

  gboolean watches_running;
  void (*start_watches)(DirectoryMonitor *self);
  void (*stop_watches)(DirectoryMonitor *self);
  void (*free_fn)(DirectoryMonitor *self);
};

DirectoryMonitor *directory_monitor_new(const gchar *dir, guint recheck_time);
void directory_monitor_init_instance(DirectoryMonitor *self, const gchar *dir, guint recheck_time);
void directory_monitor_free(DirectoryMonitor *self);
void directory_monitor_set_callback(DirectoryMonitor *self, DirectoryMonitorEventCallback callback, gpointer user_data);

void directory_monitor_start(DirectoryMonitor *self);
void directory_monitor_stop(DirectoryMonitor *self);

void directory_monitor_stop_and_destroy(DirectoryMonitor *self);
void directory_monitor_schedule_destroy(DirectoryMonitor *self);

gchar *build_filename(const gchar *basedir, const gchar *path);

#endif /* MODULES_AFFILE_DIRECTORY_MONITOR_H_ */
