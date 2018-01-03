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

enum
{
  LPT_EXTRACTED    = 0x0001,
  LPT_WAITING      = 0x0002,
  LPT_CONSUME_LINE = 0x0010,
  LPT_REWIND_LINE  = 0x0020,
};

#define LPT_CONSUME_PARTIAL_AMOUNT_SHIFT     8
#define LPT_CONSUME_PARTIAL_AMOUNT_MASK      ~0xFF
#define LPT_CONSUME_PARTIALLY(drop_length) (LPT_CONSUME_LINE | ((drop_length) << LPT_CONSUME_PARTIAL_AMOUNT_SHIFT))

typedef struct _LogProtoTextServer LogProtoTextServer;
struct _LogProtoTextServer
{
  LogProtoBufferedServer super;

  gint (*accumulate_line)(LogProtoTextServer *self,
                          const guchar *msg,
                          gsize msg_len,
                          gssize consumed_len);

  GIConv reverse_convert;
  gchar *reverse_buffer;
  gsize reverse_buffer_len;
  gint convert_scale;

  gint32 consumed_len;
  gint32 cached_eol_pos;
};

/* LogProtoTextServer
 *
 * This class processes text files/streams. Each record is terminated via an EOL character.
 */
LogProtoServer *log_proto_text_server_new(LogTransport *transport, const LogProtoServerOptions *options);
void log_proto_text_server_init(LogProtoTextServer *self, LogTransport *transport,
                                const LogProtoServerOptions *options);

static inline gint
log_proto_text_server_accumulate_line(LogProtoTextServer *self,
                                      const guchar *msg,
                                      gsize msg_len,
                                      gssize consumed_len)
{
  return self->accumulate_line(self, msg, msg_len, consumed_len);
}

gint log_proto_get_char_size_for_fixed_encoding(const gchar *encoding);

static inline gboolean
log_proto_text_server_validate_options_method(LogProtoServer *s)
{
  return log_proto_buffered_server_validate_options_method(s);
}

#endif
