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
#ifndef MODULES_AFFILE_FILE_READER_H_
#define MODULES_AFFILE_FILE_READER_H_

#include "driver.h"
#include "logreader.h"
#include "logproto/logproto-regexp-multiline-server.h"
#include "affile-common.h"

enum
{
  MLM_NONE,
  MLM_INDENTED,
  MLM_PREFIX_GARBAGE,
  MLM_PREFIX_SUFFIX,
};

typedef struct _FileReaderOptions {
  FilePermOptions file_perm_options;
  FileOpenOptions file_open_options;
  gint pad_size;
  gint follow_freq;
  gint multi_line_mode;
  MultiLineRegexp *multi_line_prefix, *multi_line_garbage;
  LogReaderOptions reader_options;
} FileReaderOptions;

typedef struct _FileReader
{
  LogPipe super;
  LogSrcDriver *owner;
  GString *filename;
  FileReaderOptions *file_reader_options;
  LogReader *reader;
} FileReader;

FileReader *file_reader_new(gchar *filename, LogSrcDriver *owner, GlobalConfig *cfg);

void file_reader_options_set_follow_freq(FileReaderOptions *options, gint follow_freq);
gboolean file_reader_options_set_multi_line_mode(FileReaderOptions *options, const gchar *mode);
gboolean file_reader_options_set_multi_line_prefix(FileReaderOptions *options, const gchar *prefix_regexp, GError **error);
gboolean file_reader_options_set_multi_line_garbage(FileReaderOptions *options, const gchar *prefix_regexp, GError **error);

void file_reader_options_destroy(FileReaderOptions *options);


#endif /* MODULES_AFFILE_FILE_READER_H_ */
