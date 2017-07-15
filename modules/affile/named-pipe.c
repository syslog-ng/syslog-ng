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
#include "transport/transport-pipe.h"
#include "logproto/logproto-text-client.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static gboolean
_prepare_open(FileOpener *self, const gchar *name)
{
  struct stat st;

  if (stat(name, &st) < 0 &&
      (errno == ENOENT || errno == ENOTDIR))
    {
      if (mkfifo(name, self->options->file_perm_options.file_perm) < 0)
        {
          msg_error("Error creating named pipe, mkfifo() returned an error",
                    evt_tag_str("file", name),
                    evt_tag_str("error", g_strerror(errno)));
          return FALSE;
        }
      return TRUE;
    }

  if (!S_ISFIFO(st.st_mode))
    {
      msg_error("You are using the pipe() driver, underlying file is not a FIFO, it should be used by file()",
                evt_tag_str("filename", name));
      errno = EINVAL;
      return FALSE;
    }
  return TRUE;
}

static gint
_get_open_flags(FileOpener *self, FileDirection dir)
{
  switch (dir)
    {
    case AFFILE_DIR_READ:
    case AFFILE_DIR_WRITE:
      return (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE);
    default:
      g_assert_not_reached();
    }
}

static LogTransport *
_construct_transport(FileOpener *self, gint fd)
{
  LogTransport *transport = log_transport_pipe_new(fd);

  transport->read = log_transport_file_read_and_ignore_eof_method;
  return transport;
}

static LogProtoServer *
_construct_src_proto(FileOpener *s, LogTransport *transport, LogProtoFileReaderOptions *proto_options)
{
  return log_proto_file_reader_new(transport, proto_options);
}

static LogProtoClient *
_construct_dst_proto(FileOpener *self, LogTransport *transport, LogProtoClientOptions *proto_options)
{
  return log_proto_text_client_new(transport, proto_options);
}

FileOpener *
file_opener_for_named_pipes_new(void)
{
  FileOpener *self = file_opener_new();

  self->prepare_open = _prepare_open;
  self->get_open_flags = _get_open_flags;
  self->construct_transport = _construct_transport;
  self->construct_src_proto = _construct_src_proto;
  self->construct_dst_proto = _construct_dst_proto;
  return self;
}
