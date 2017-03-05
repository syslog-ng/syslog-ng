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

typedef enum {
  FILE_IS_DIRECTORY,
  FILE_IS_REGULAR
} FileType;

typedef struct _DirectoryMonitorEvent {
  const gchar *name;
  const gchar *full_path;
  FileType file_type;
} DirectoryMonitorEvent;

typedef  void (*COLLECT_FILES_CALLBACK)(const DirectoryMonitorEvent *event,
                                        gpointer user_data);

typedef struct _DirectoryMonitor {
  gchar *dir;
} DirectoryMonitor;

DirectoryMonitor* directory_monitor_new(const gchar *dir);
void directory_monitor_free(DirectoryMonitor *self);

void directory_monitor_collect_all_files(DirectoryMonitor *self, COLLECT_FILES_CALLBACK, gpointer user_data);

#endif /* MODULES_AFFILE_DIRECTORY_MONITOR_H_ */
