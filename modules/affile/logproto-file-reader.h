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

#include "logproto/logproto-text-server.h"
#include "multi-line/multi-line-factory.h"

typedef struct _LogProtoFileReaderOptions
{
  LogProtoServerOptions super;
  gint pad_size;
} LogProtoFileReaderOptions;

typedef union _LogProtoFileReaderOptionsStorage
{
  LogProtoServerOptionsStorage storage;
  LogProtoFileReaderOptions super;

} LogProtoFileReaderOptionsStorage;
// _Static_assert() is a C11 feature, so we use a typedef trick to perform the static assertion
typedef char static_assert_size_check_LogProtoFileReaderOptions[
  sizeof(LogProtoServerOptionsStorage) >= sizeof(LogProtoFileReaderOptions) ? 1 : -1];

LogProtoServer *log_proto_file_reader_new(LogTransport *transport, const LogProtoFileReaderOptionsStorage *options);

void log_proto_file_reader_options_defaults(LogProtoFileReaderOptionsStorage *options);
gboolean log_proto_file_reader_options_init(LogProtoFileReaderOptionsStorage *options, GlobalConfig *cfg);
void log_proto_file_reader_options_destroy(LogProtoFileReaderOptionsStorage *options);

#endif
