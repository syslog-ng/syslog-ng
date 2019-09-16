/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balázs Scheidler
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

#ifndef LOG_PROTO_FILE_READER_H_INCLUDED
#define LOG_PROTO_FILE_READER_H_INCLUDED

#include "logproto/logproto-multiline-server.h"

typedef struct _LogProtoFileReaderOptions
{
  LogProtoMultiLineServerOptions super;
  gint pad_size;
} LogProtoFileReaderOptions;

LogProtoServer *log_proto_file_reader_new(LogTransport *transport, const LogProtoFileReaderOptions *options);

void log_proto_file_reader_options_defaults(LogProtoFileReaderOptions *options);
gboolean log_proto_file_reader_options_init(LogProtoFileReaderOptions *options);
void log_proto_file_reader_options_destroy(LogProtoFileReaderOptions *options);


#endif
