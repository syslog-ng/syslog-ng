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
#ifndef MODULES_AFFILE_DIRECTORY_MONITOR_CONTENT_COMPARATOR_H_
#define MODULES_AFFILE_DIRECTORY_MONITOR_CONTENT_COMPARATOR_H_

#include <collection-comparator.h>
#include "directory-monitor.h"

typedef struct _DirectoryMonitorContentComparator
{
  DirectoryMonitor super;
  CollectionComparator *comparator;
} DirectoryMonitorContentComparator;

DirectoryMonitor *directory_monitor_content_comparator_new(void);
void directory_monitor_content_comparator_init_instance(DirectoryMonitorContentComparator *self, const gchar *dir,
                                                        guint recheck_time, const gchar *method);
void directory_monitor_content_comparator_free(DirectoryMonitorContentComparator *self);
void directory_monitor_content_comparator_rescan_directory(DirectoryMonitorContentComparator *self,
                                                           gboolean initial_scan);

#endif /* MODULES_AFFILE_DIRECTORY_MONITOR_CONTENT_COMPARATOR_H_ */
