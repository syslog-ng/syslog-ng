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

#include "logproto-file-reader.h"
#include "logproto/logproto-record-server.h"
#include "logproto/logproto-multiline-server.h"
#include "messages.h"

LogProtoServer *
log_proto_file_reader_new(LogTransport *transport, const LogProtoFileReaderOptions *options)
{
  if (options->pad_size > 0)
    return log_proto_padded_record_server_new(transport, &options->super.super, options->pad_size);
  else
    return log_proto_multiline_server_new(transport, &options->super);
}

void
log_proto_file_reader_options_defaults(LogProtoFileReaderOptions *options)
{
  log_proto_multi_line_server_options_defaults(&options->super);
  options->pad_size = 0;
}

static gboolean
_are_multi_line_settings_invalid(LogProtoFileReaderOptions *options)
{
  gboolean is_garbage_mode = options->super.mode == MLM_PREFIX_GARBAGE;
  gboolean is_suffix_mode = options->super.mode == MLM_PREFIX_SUFFIX;

  return (!is_garbage_mode && !is_suffix_mode) && (options->super.prefix
                                                   || options->super.garbage);
}

static gboolean
log_proto_file_reader_options_validate(LogProtoFileReaderOptions *options)
{
  if (_are_multi_line_settings_invalid(options))
    {
      msg_error("multi-line-prefix() and/or multi-line-garbage() specified but multi-line-mode() is not regexp based "
                "(prefix-garbage or prefix-suffix), please set multi-line-mode() properly");
      return FALSE;
    }

  if (options->pad_size > 0 && options->super.mode != MLM_NONE)
    {
      msg_error("pad-size() and multi-line-mode() can not be used together");
      return FALSE;
    }

  return TRUE;
}

gboolean
log_proto_file_reader_options_init(LogProtoFileReaderOptions *options)
{
  log_proto_multi_line_server_options_init(&options->super);
  return log_proto_file_reader_options_validate(options);
}

void
log_proto_file_reader_options_destroy(LogProtoFileReaderOptions *options)
{
  log_proto_multi_line_server_options_destroy(&options->super);
}
