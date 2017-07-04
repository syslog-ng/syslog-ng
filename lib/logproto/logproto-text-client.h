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
#ifndef LOGPROTO_TEXT_CLIENT_H_INCLUDED
#define LOGPROTO_TEXT_CLIENT_H_INCLUDED

#include "logproto-client.h"

typedef struct _LogProtoTextClient
{
  LogProtoClient super;
  gint state, next_state;
  guchar *partial;
  GDestroyNotify partial_free;
  gsize partial_len, partial_pos;
} LogProtoTextClient;

LogProtoStatus log_proto_text_client_submit_write(LogProtoClient *s, guchar *msg, gsize msg_len,
                                                  GDestroyNotify msg_free, gint next_state);
void log_proto_text_client_init(LogProtoTextClient *self, LogTransport *transport,
                                const LogProtoClientOptions *options);
LogProtoClient *log_proto_text_client_new(LogTransport *transport, const LogProtoClientOptions *options);

#define log_proto_text_client_free_method log_proto_client_free_method

#endif
