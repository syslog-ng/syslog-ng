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
#ifndef MODULES_AFFILE_WILDCARD_SOURCE_H_
#define MODULES_AFFILE_WILDCARD_SOURCE_H_

#include "syslog-ng.h"
#include "driver.h"
#include "wildcard-file-reader.h"
#include "file-list.h"
#include "directory-monitor.h"
#include "directory-monitor-factory.h"

#define MINIMUM_WINDOW_SIZE 100
#define DEFAULT_MAX_FILES 100

typedef struct _WildcardSourceDriver
{
  LogSrcDriver super;
  gchar *base_dir;
  gchar *filename_pattern;
  MonitorMethod monitor_method;
  guint32 max_files;

  gboolean window_size_initialized;
  gboolean recursive;

  FileReaderOptions file_reader_options;
  FileOpenerOptions file_opener_options;

  GPatternSpec *compiled_pattern;
  GHashTable *file_readers;
  GHashTable *directory_monitors;
  FileOpener *file_opener;
  FileStateEvent deleted_file_events;

  PendingFileList *waiting_list;
} WildcardSourceDriver;

LogDriver *wildcard_sd_new(GlobalConfig *cfg);

void wildcard_sd_set_base_dir(LogDriver *s, const gchar *base_dir);
void wildcard_sd_set_filename_pattern(LogDriver *s, const gchar *filename_pattern);
void wildcard_sd_set_recursive(LogDriver *s, gboolean recursive);
gboolean wildcard_sd_set_monitor_method(LogDriver *s, const gchar *method);
void wildcard_sd_set_max_files(LogDriver *s, guint32 max_files);

#endif /* MODULES_AFFILE_WILDCARD_SOURCE_H_ */
