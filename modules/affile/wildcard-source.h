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
#include "file-reader.h"
#include "directory-monitor.h"

typedef struct _WildcardSourceDriver {
  LogSrcDriver super;
  GString *base_dir;
  GString *filename_pattern;
  gboolean recursive;
  gboolean force_dir_polling;
  guint32 max_files;

  FileReaderOptions file_reader_options;

  GPatternSpec *compiled_pattern;
  GHashTable *file_readers;
  DirectoryMonitor *directory_monitor;
} WildcardSourceDriver;

LogDriver *wildcard_sd_new(GlobalConfig *cfg);

void wildcard_sd_set_base_dir(LogDriver *s, const gchar *base_dir);
void wildcard_sd_set_filename_pattern(LogDriver *s, const gchar *filename_pattern);
void wildcard_sd_set_recursive(LogDriver *s, gboolean recursive);
void wildcard_sd_set_force_directory_polling(LogDriver *s, gboolean force_directory_polling);
void wildcard_sd_set_max_files(LogDriver *s, guint32 max_files);

#endif /* MODULES_AFFILE_WILDCARD_SOURCE_H_ */
