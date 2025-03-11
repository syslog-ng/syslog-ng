/*
 * Copyright (c) 2025 One Identity
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
#ifndef LOGPROTO_HTTP_SERVER_INCLUDED
#define LOGPROTO_HTTP_SERVER_INCLUDED

#include "logproto/logproto-server.h"
#include "logproto/logproto-text-server.h"

typedef struct _LogProtoHTTPServerOptions
{
  LogProtoServerOptions super; // This must be the first !!!
  gboolean initialized;
  gboolean close_after_send;

} LogProtoHTTPServerOptions;

typedef union _LogProtoHTTPServerOptionsStorage
{
  LogProtoServerOptionsStorage storage;
  LogProtoHTTPServerOptions super;

} LogProtoHTTPServerOptionsStorage;

typedef struct _LogProtoHTTPServer LogProtoHTTPServer;
struct _LogProtoHTTPServer
{
  LogProtoTextServer super;
  const LogProtoHTTPServerOptionsStorage *options;

  GString *(*request_processor)(LogProtoHTTPServer *s, LogProtoBufferedServerState *state,
                                const guchar *buffer_start, gsize buffer_bytes);
  gboolean (*request_header_checker)(LogProtoHTTPServer *self, gchar *buffer_start, gsize buffer_bytes);
  GString *(*response_header_composer)(LogProtoHTTPServer *self, const gchar *data, gsize data_len,
                                       gboolean close_after_sent);
  GString *(*response_body_composer)(LogProtoHTTPServer *self);
  gssize (*response_sender)(LogProtoHTTPServer *self, const gchar *data, gsize data_len, gboolean close_after_sent);
};

void log_proto_http_server_options_defaults(LogProtoHTTPServerOptionsStorage *options);
void log_proto_http_server_options_init(LogProtoHTTPServerOptionsStorage *options, GlobalConfig *cfg);
void log_proto_http_server_destroy(LogProtoHTTPServerOptionsStorage *options);
void log_proto_http_server_options_set_close_after_send(
  LogProtoHTTPServerOptionsStorage *options,
  gboolean value);

LogProtoServer *log_proto_http_server_new(LogTransport *transport,
                                          const LogProtoServerOptionsStorage *options);
void log_proto_http_server_init(LogProtoHTTPServer *self, LogTransport *transport,
                                const LogProtoHTTPServerOptionsStorage *options);

#endif
