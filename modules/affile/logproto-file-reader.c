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
#include "messages.h"

LogProtoServer *
log_proto_file_reader_new(LogTransport *transport, const LogProtoFileReaderOptions *options)
{
  if (options->pad_size > 0)
    return log_proto_padded_record_server_new(transport, (LogProtoServerOptionsStorage *)options, options->pad_size);
  else
    return log_proto_text_multiline_server_new(transport, (LogProtoServerOptionsStorage *)options);
}

/* TODO: these functions only initialize the fields added on top of
 * LogProtoServerOptionsStorage, the rest is the responsibility of the LogReader.
 * This whole Options structure has become very messy. There are a lot of them.
 *
 *   FileReaderOptions ->
 *       LogReaderOptions ->
 *           LogProtoServerOptions
 *              This is actually a LogProtoServerOptionsStorage structure
 *              which holds a LogProtoFileReaderOptions instance and the
 *              LogReader only stores it.
 *
 * FileOpenerOptions is fortunately independent of this mess.
 *
 * LogProtoFileReaderOptions only needs to take care about its "extra"
 * fields on top of LogProtoServerOptionsStorage, that's why we don't call anything
 * from the "inherited" class.  This is because that class is
 * defaulted/initialized/destroyed by LogReader.  This layer just manages
 * what we happen to store in addition to LogProtoServerOptionsStorage.
 *
 * You have been warned!
 *
 * PS: I attempted to refactor this, but at first try it became more messy, so I
 * abandoned it, but it should be done eventually.
 **/

void
log_proto_file_reader_options_defaults(LogProtoFileReaderOptions *options)
{
  options->pad_size = 0;
}

static gboolean
log_proto_file_reader_options_validate(LogProtoFileReaderOptions *options)
{
  if (options->pad_size > 0 && options->super.multi_line_options.mode != MLM_NONE)
    {
      msg_error("pad-size() and multi-line-mode() can not be used together");
      return FALSE;
    }

  return TRUE;
}

gboolean
log_proto_file_reader_options_init(LogProtoFileReaderOptions *options, GlobalConfig *cfg)
{
  return log_proto_file_reader_options_validate(options);
}
