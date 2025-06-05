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
#include "affile-source.h"

#include <unistd.h>

static LogTransport *
_construct_transport(FileOpener *self, gint fd)
{
  return log_transport_file_new(fd);
}

static LogProtoServer *
_construct_src_proto(FileOpener *s, LogTransport *transport, LogProtoFileReaderOptionsStorage *proto_options)
{
  return log_proto_file_reader_new(transport, proto_options);
}

static gint
_open(FileOpener *self, const gchar *name, gint flags)
{
  return dup(0);
}

FileOpener *
file_opener_for_stdin_new(void)
{
  FileOpener *self = file_opener_new();

  self->construct_transport = _construct_transport;
  self->construct_src_proto = _construct_src_proto;
  self->open = _open;
  return self;
}

gboolean
stdin_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (affile_sd_init(s))
    {
      self->file_reader->super.generate_persist_name = NULL;
      return TRUE;
    }
  return FALSE;
}

LogDriver *
stdin_sd_new(GlobalConfig *cfg)
{
  AFFileSourceDriver *self = affile_sd_new_instance("/dev/stdin", cfg);

  self->super.super.super.init = stdin_sd_init;

  self->file_reader_options.restore_state = FALSE;
  self->file_reader_options.reader_options.flags |= LR_EXIT_ON_EOF;
  self->file_reader_options.reader_options.super.stats_source = stats_register_type("stdin");
  self->file_opener = file_opener_for_stdin_new();
  affile_sd_set_transport_name(self, "local+stdin");
  return &self->super.super;
}
