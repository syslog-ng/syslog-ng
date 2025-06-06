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

typedef struct _FileReaderOptions
{
  gint follow_freq;
  FollowMethod follow_method;
  gint multi_line_timeout;
  gboolean restore_state;
  LogReaderOptions reader_options;
} FileReaderOptions;

typedef struct _FileReader FileReader;
struct _FileReader
{
  LogPipe super;
  LogSrcDriver *owner;
  GString *filename;
  FileReaderOptions *options;
  FileOpener *opener;
  LogReader *reader;
  const gchar *persist_name;
  const gchar *persist_name_prefix;
  gboolean monitor_can_notify_file_changes;

  void (*on_file_moved)(FileReader *);
};

static inline LogProtoFileReaderOptionsStorage *
file_reader_options_get_log_proto_options(FileReaderOptions *options)
{
  return (LogProtoFileReaderOptionsStorage *) &options->reader_options.proto_options;
}

FileReader *file_reader_new(const gchar *filename, FileReaderOptions *options, FileOpener *opener, LogSrcDriver *owner,
                            GlobalConfig *cfg);

void file_reader_init_instance(FileReader *self, const gchar *filename, FileReaderOptions *options, FileOpener *opener,
                               LogSrcDriver *owner, GlobalConfig *cfg, const gchar *persist_name_prefix);

gboolean file_reader_init_method(LogPipe *s);
gboolean file_reader_deinit_method(LogPipe *s);
void file_reader_free_method(LogPipe *s);
void file_reader_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options);
gint file_reader_notify_method(LogPipe *s, gint notify_code, gpointer user_data);

void file_reader_remove_persist_state(FileReader *self);
void file_reader_stop_follow_file(FileReader *self);
void file_reader_cue_buffer_flush(FileReader *self);

gboolean iv_can_poll_fd(gint fd);

void file_reader_options_set_follow_freq(FileReaderOptions *options, gint follow_freq);
gboolean file_reader_options_set_follow_method(FileReaderOptions *options, const gchar *follow_method);
void file_reader_options_set_multi_line_timeout(FileReaderOptions *options, gint multi_line_timeout);

void file_reader_options_defaults(FileReaderOptions *options);
gboolean file_reader_options_init(FileReaderOptions *options, GlobalConfig *cfg, const gchar *group);
void file_reader_options_deinit(FileReaderOptions *options);



#endif /* MODULES_AFFILE_FILE_READER_H_ */
