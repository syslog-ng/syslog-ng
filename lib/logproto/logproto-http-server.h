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

/* NOTE: This class is just a very-very thin, basic http request hendler.
 *       A more complex implementation, if ever needed, should have its own
 *       settings by a good chance.
 *       Now, it is added to the builtin protos and DEFINE_LOG_PROTO_SERVER
 *       can generate a proto server with the default LogProtoServerOptions
 *       so, a more complex impl. either requires modification of
 *       DEFINE_LOG_PROTO_SERVER, and probably the rewriting of the options
 *       handling, and/or will need its own grammar, etc.
 *
  // typedef struct _LogProtoHTTPServerOptions LogProtoHTTPServerOptions;
  // struct _LogProtoHTTPServerOptions
  // {
  //   LogProtoServerOptions super; // LogProtoServerOptionsStorage ???
// };
*/

typedef struct _LogProtoHTTPServer LogProtoHTTPServer;
struct _LogProtoHTTPServer
{
  LogProtoTextServer super;

  GString *(*request_processor)(LogProtoHTTPServer *s, LogProtoBufferedServerState *state,
                                const guchar *buffer_start, gsize buffer_bytes);
  gboolean (*request_header_checker)(LogProtoHTTPServer *self, gchar *buffer_start, gsize buffer_bytes);
  GString *(*response_header_composer)(LogProtoHTTPServer *self, const gchar *data, gsize data_len,
                                       gboolean close_after_sent);
  GString *(*response_body_composer)(LogProtoHTTPServer *self);
  gssize (*response_sender)(LogProtoHTTPServer *self, const gchar *data, gsize data_len, gboolean close_after_sent);
};

LogProtoServer *log_proto_http_server_new(LogTransport *transport,
                                          const LogProtoServerOptions /* LogProtoHTTPServerOptions */ *options);
void log_proto_http_server_init(LogProtoHTTPServer *self, LogTransport *transport,
                                const LogProtoServerOptions /* LogProtoHTTPServerOptions */ *options);

#endif
