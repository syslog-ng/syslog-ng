/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "affile-dest.h"
#include "logproto-file-writer.h"
#include <unistd.h>

static LogTransport *
_construct_transport(FileOpener *self, gint fd)
{
  return log_transport_file_new(fd);
}

static LogProtoClient *
_construct_dst_proto(FileOpener *s, LogTransport *transport, LogProtoClientOptions *proto_options)
{
  return log_proto_file_writer_new(transport, proto_options, 0, FALSE);
}

static gint
_open(FileOpener *self, const gchar *name, gint flags)
{
  return dup(1);
}

FileOpener *
file_opener_for_stdout_new(void)
{
  FileOpener *self = file_opener_new();

  self->construct_transport = _construct_transport;
  self->construct_dst_proto = _construct_dst_proto;
  self->open = _open;
  return self;
}

LogDriver *
stdout_dd_new(GlobalConfig *cfg)
{
  LogTemplate *filename_template = log_template_new(cfg, NULL);

  log_template_compile_literal_string(filename_template, "/dev/stdout");
  AFFileDestDriver *self = affile_dd_new_instance(filename_template, cfg);

  self->writer_options.stats_source = stats_register_type("stdout");
  self->file_opener = file_opener_for_stdout_new();
  return &self->super.super;
}
