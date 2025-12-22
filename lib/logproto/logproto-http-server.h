/*
 * Copyright (c) 2025 One Identity
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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

static const gchar http_too_many_request_msg[] = "HTTP/1.1 429 Too Many Requests";
static const gchar http_bad_request_msg[] = "HTTP/1.1 400 Bad Request";
static const gchar http_ok_msg[] = "HTTP/1.1 200 OK";

typedef struct _LogProtoHTTPServerOptions
{
  LogProtoServerOptions super;
  gboolean close_after_send;

} LogProtoHTTPServerOptions;

typedef union _LogProtoHTTPServerOptionsStorage
{
  LogProtoServerOptionsStorage storage;
  LogProtoHTTPServerOptions super;

} LogProtoHTTPServerOptionsStorage;
// _Static_assert() is a C11 feature, so we use a typedef trick to perform the static assertion
typedef char static_assert_size_check_LogProtoHTTPServerOptions[
  sizeof(LogProtoServerOptionsStorage) >= sizeof(LogProtoHTTPServerOptions) ? 1 : -1];

typedef struct _LogProtoHTTPServer LogProtoHTTPServer;
struct _LogProtoHTTPServer
{
  LogProtoTextServer super;
  const LogProtoHTTPServerOptionsStorage *options;

  GString *(*request_processor)(LogProtoHTTPServer *s, LogProtoBufferedServerState *state,
                                const guchar *buffer_start, gsize buffer_bytes);
  gint (*request_header_checker)(LogProtoHTTPServer *self, gchar *buffer_start, gsize buffer_bytes);
  GString *(*response_header_composer)(LogProtoHTTPServer *self, const gchar *data, gsize data_len,
                                       gboolean close_after_sent);
  GString *(*response_body_composer)(LogProtoHTTPServer *self);
  gssize (*response_sender)(LogProtoHTTPServer *self, const gchar *data, gsize data_len, gboolean close_after_sent);
};

void log_proto_http_server_options_defaults(LogProtoServerOptionsStorage *options);
void log_proto_http_server_options_init(LogProtoServerOptionsStorage *options, GlobalConfig *cfg);
void log_proto_http_server_options_destroy(LogProtoServerOptionsStorage *options);
gboolean log_proto_http_server_options_validate(LogProtoServerOptionsStorage *options);
void log_proto_http_server_options_set_close_after_send(
  LogProtoServerOptionsStorage *options,
  gboolean value);

LogProtoServer *log_proto_http_server_new(LogTransport *transport,
                                          const LogProtoServerOptionsStorage *options);
void log_proto_http_server_init(LogProtoHTTPServer *self, LogTransport *transport,
                                const LogProtoServerOptionsStorage *options);

#endif
