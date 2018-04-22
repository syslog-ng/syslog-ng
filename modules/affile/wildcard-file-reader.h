/*
 * Copyright (c) 2018 Balabit
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


#ifndef MODULES_AFFILE_WILDCARD_FILE_READER_H_
#define MODULES_AFFILE_WILDCARD_FILE_READER_H_

#include "file-reader.h"
#include <iv.h>

typedef struct _WildcardFileReader WildcardFileReader;

typedef void (*FileStateEventCallback)(FileReader *file_reader, gpointer user_data);

typedef struct _FileStateEvent
{
  FileStateEventCallback deleted_file_finised;
  FileStateEventCallback deleted_file_eof;
  gpointer user_data;
} FileStateEvent;

typedef struct _FileState
{
  gboolean deleted;
  gboolean eof;
  gboolean last_msg_sent;
} FileState;


struct _WildcardFileReader
{
  FileReader super;
  FileState file_state;
  FileStateEvent *file_state_event;
  struct iv_task file_state_event_handler;
};

FileReader *
wildcard_file_reader_new(const gchar *filename, FileReaderOptions *options,
                         FileOpener *opener, LogSrcDriver *owner,
                         GlobalConfig *cfg, FileStateEvent *file_state_event);


#endif /* MODULES_AFFILE_WILDCARD_FILE_READER_H_ */
