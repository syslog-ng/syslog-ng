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
#include "file-specializations.h"
#include "transport/transport-file.h"
#include "logproto-file-writer.h"
#include "messages.h"
#include "ack-tracker/ack_tracker_factory.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static gboolean
_prepare_open(FileOpener *self, const gchar *name)
{
  struct stat st;

  if (stat(name, &st) >= 0)
    {
      if (S_ISFIFO(st.st_mode))
        {
          msg_error("You are using the file() driver, underlying file is a FIFO, it should be used by pipe()",
                    evt_tag_str("filename", name));
          errno = EINVAL;
          return FALSE;
        }
    }
  return TRUE;
}

static LogTransport *
_construct_src_transport(FileOpener *self, gint fd)
{
  LogTransport *transport = log_transport_file_new(fd);

  transport->read = log_transport_file_read_and_ignore_eof_method;
  return transport;
}

static LogProtoServer *
_construct_src_proto(FileOpener *s, LogTransport *transport, LogProtoFileReaderOptions *proto_options)
{
  log_proto_server_options_set_ack_tracker_factory(&proto_options->super.super,
                                                   consecutive_ack_tracker_factory_new());
  return log_proto_file_reader_new(transport, proto_options);
}

FileOpener *
file_opener_for_regular_source_files_new(void)
{
  FileOpener *self = file_opener_new();

  self->prepare_open = _prepare_open;
  self->construct_transport = _construct_src_transport;
  self->construct_src_proto = _construct_src_proto;
  return self;
}

typedef struct _FileOpenerRegularDestFiles
{
  FileOpener super;
  const LogWriterOptions *writer_options;
  gboolean *use_fsync;
} FileOpenerRegularDestFiles;

static LogProtoClient *
_construct_dst_proto(FileOpener *s, LogTransport *transport, LogProtoClientOptions *proto_options)
{
  FileOpenerRegularDestFiles *self = (FileOpenerRegularDestFiles *) s;

  return log_proto_file_writer_new(transport, proto_options,
                                   self->writer_options->flush_lines,
                                   *self->use_fsync);
}

static LogTransport *
_construct_transport(FileOpener *s, gint fd)
{
  return log_transport_file_new(fd);
}

FileOpener *
file_opener_for_regular_dest_files_new(const LogWriterOptions *writer_options, gboolean *use_fsync)
{
  FileOpenerRegularDestFiles *self = g_new0(FileOpenerRegularDestFiles, 1);

  file_opener_init_instance(&self->super);
  self->super.construct_transport = _construct_transport;
  self->super.construct_dst_proto = _construct_dst_proto;
  self->writer_options = writer_options;
  self->use_fsync = use_fsync;
  return &self->super;
}
