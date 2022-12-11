/*
 * Copyright (c) 2002-2012 Balabit
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
#include "multi-line/multi-line-logic.h"

typedef struct _LogProtoTextServer LogProtoTextServer;
struct _LogProtoTextServer
{
  LogProtoBufferedServer super;
  MultiLineLogic *multi_line;

  const guchar *(*find_eom)(const guchar *s, gsize n);
  gint32 consumed_len;
  gint32 cached_eol_pos;
};

void log_proto_text_server_set_multi_line(LogProtoServer *s, MultiLineLogic *multi_line);

/* LogProtoTextServer
 *
 * This class processes text files/streams. Each record is terminated via an EOL character.
 */
LogProtoServer *log_proto_text_server_new(LogTransport *transport, const LogProtoServerOptions *options);
LogProtoServer *log_proto_text_with_nuls_server_new(LogTransport *transport, const LogProtoServerOptions *options);

void log_proto_text_server_free(LogProtoServer *self);
void log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport,
                                const LogProtoServerOptions *options);
LogProtoPrepareAction log_proto_text_server_prepare_method(LogProtoServer *s, GIOCondition *cond, gint *timeout);

static inline gboolean
log_proto_text_server_validate_options_method(LogProtoServer *s)
{
  return log_proto_buffered_server_validate_options_method(s);
}

#endif
