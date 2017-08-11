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
#include "transport-prockmsg.h"
#include "messages.h"
#include "logproto/logproto-dgram-server.h"
#include "logproto/logproto-text-server.h"

#include <unistd.h>
#include <errno.h>

/**************************************************************
 * /dev/kmsg support
 **************************************************************/


LogTransport *
log_transport_devkmsg_new(gint fd)
{
  if (lseek(fd, 0, SEEK_END) < 0)
    {
      msg_error("Error seeking /dev/kmsg to the end",
                evt_tag_str("error", g_strerror(errno)));
    }
  return log_transport_file_new(fd);
}


static LogTransport *
_construct_devkmsg_transport(FileOpener *self, gint fd)
{
  return log_transport_devkmsg_new(fd);
}

static LogProtoServer *
_construct_devkmsg_proto(FileOpener *self, LogTransport *transport, LogProtoFileReaderOptions *options)
{
  return log_proto_dgram_server_new(transport, &options->super.super);
}

FileOpener *
file_opener_for_devkmsg_new(void)
{
  FileOpener *self = file_opener_new();

  self->construct_transport = _construct_devkmsg_transport;
  self->construct_src_proto = _construct_devkmsg_proto;
  return self;
}

/**************************************************************
 * /proc/kmsg support
 **************************************************************/

static LogProtoServer *
log_proto_linux_proc_kmsg_reader_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoServer *proto;

  proto = log_proto_text_server_new(transport, options);
  ((LogProtoTextServer *) proto)->super.no_multi_read = TRUE;
  return proto;
}


static LogTransport *
_construct_prockmsg_transport(FileOpener *self, gint fd)
{
  return log_transport_prockmsg_new(fd, 10);
}

static LogProtoServer *
_construct_prockmsg_proto(FileOpener *self, LogTransport *transport, LogProtoFileReaderOptions *options)
{
  return log_proto_linux_proc_kmsg_reader_new(transport, &options->super.super);
}

FileOpener *
file_opener_for_prockmsg_new(void)
{
  FileOpener *self = file_opener_new();

  self->construct_transport = _construct_prockmsg_transport;
  self->construct_src_proto = _construct_prockmsg_proto;
  return self;
}
