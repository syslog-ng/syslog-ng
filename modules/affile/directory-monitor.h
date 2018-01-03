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
#ifndef MODULES_AFFILE_DIRECTORY_MONITOR_H_
#define MODULES_AFFILE_DIRECTORY_MONITOR_H_

#include <syslog-ng.h>
#include <iv.h>

typedef enum
{
  FILE_CREATED,
  DIRECTORY_CREATED,
  DELETED,
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

gchar *build_filename(const gchar *basedir, const gchar *path);

#endif /* MODULES_AFFILE_DIRECTORY_MONITOR_H_ */
