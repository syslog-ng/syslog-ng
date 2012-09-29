/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#ifndef LOGPROTO_TEXT_SERVER_INCLUDED
#define LOGPROTO_TEXT_SERVER_INCLUDED

#include "logproto-buffered-server.h"

typedef struct _LogProtoTextServer LogProtoTextServer;
struct _LogProtoTextServer
{
  LogProtoBufferedServer super;
  GIConv reverse_convert;
  gchar *reverse_buffer;
  gsize reverse_buffer_len;
  gint convert_scale;
};

/* LogProtoTextServer
 *
 * This class processes text files/streams. Each record is terminated via an EOL character.
 */
LogProtoServer *log_proto_text_server_new(LogTransport *transport, const LogProtoServerOptions *options);
void log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport, const LogProtoServerOptions *options);

gint log_proto_get_char_size_for_fixed_encoding(const gchar *encoding);

#endif
