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
#include "file-opener.h"
#include "iv_event.h"
#include "file-state-handler.h"

typedef struct _FileReaderOptions
{
  gint follow_freq;
  gboolean restore_state;
  LogReaderOptions reader_options;
  gboolean exit_on_eof;
} FileReaderOptions;



typedef struct _FileReader
{
  LogPipe super;
  LogSrcDriver *owner;
  GString *filename;
  FileReaderOptions *options;
  FileOpener *opener;
  LogReader *reader;
  FileStateHandler *file_state;
} FileReader;

static inline LogProtoFileReaderOptions *
file_reader_options_get_log_proto_options(FileReaderOptions *options)
{
  return (LogProtoFileReaderOptions *) &options->reader_options.proto_options;
}

FileReader *file_reader_new(const gchar *filename, FileReaderOptions *options, FileOpener *opener, LogSrcDriver *owner,
                            GlobalConfig *cfg, FileStateHandler *file_state);

void file_reader_remove_persist_state(FileReader *self);
void file_reader_stop_follow_file(FileReader *self);

void file_reader_options_set_follow_freq(FileReaderOptions *options, gint follow_freq);

void file_reader_options_defaults(FileReaderOptions *options);
void file_reader_options_init(FileReaderOptions *options, GlobalConfig *cfg, const gchar *group);
void file_reader_options_deinit(FileReaderOptions *options);



#endif /* MODULES_AFFILE_FILE_READER_H_ */
