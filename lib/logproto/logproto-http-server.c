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
#include "logproto/logproto-http-server.h"
#include "messages.h"

#include <unistd.h>

static GString *
_compose_response_body(LogProtoHTTPServer *self)
{
  return NULL;
}

static gboolean
_check_request_headers(LogProtoHTTPServer *self, gchar *buffer_start, gsize buffer_bytes)
{
  return FALSE;
}

static GString *
_http_request_processor(LogProtoHTTPServer *self, LogProtoBufferedServerState *state,
                        const guchar *buffer_start, gsize buffer_bytes)
{
  GString *response_data = NULL;

  if (self->request_header_checker(self, (gchar *)buffer_start, buffer_bytes))
    response_data = self->response_body_composer(self);
  return response_data;
}

static GString *
_compose_response_header(LogProtoHTTPServer *self, const gchar *data, gsize data_len, gboolean close_after_sent)
{
  const gint maxContentNumLength = 5;
  const gchar close_str[] = "Connection: close\n";
  const gchar response_header_fmt[] = "HTTP/1.1 200 OK\n"
                                      "Content-Type: text/plain; version=0.0.4\n"
                                      "Content-Length: %*lu\n"
                                      "%s\n"
                                      "\n";
  GString *response = g_string_sized_new(G_N_ELEMENTS(response_header_fmt) - 1 +
                                         G_N_ELEMENTS(close_str) - 1 - 2 +
                                         -4 + maxContentNumLength + data_len);
  g_string_printf(response, response_header_fmt, maxContentNumLength, data_len, close_after_sent ? close_str : "");
  return response;
}

static gssize
_send_response(LogProtoHTTPServer *self, const gchar *data, gsize data_len, gboolean close_after_sent)
{
  GString *response = self->response_header_composer(self, data, data_len, close_after_sent);
  if (response && data && data > 0)
    g_string_append(response, data);

  gssize sent_bytes = -1;
  if (response && response->len)
    sent_bytes = self->super.super.super.transport->write(self->super.super.super.transport, response->str, response->len);

  if (response)
    g_string_free(response, TRUE);
  return sent_bytes;
}

static void
_http_request_handler(LogProtoTextServer *s, LogProtoBufferedServerState *state,
                      const guchar *buffer_start, gsize buffer_bytes)
{
  LogProtoHTTPServer *self = (LogProtoHTTPServer *)s;

  GString *response = self->request_processor(self, state, buffer_start, buffer_bytes);
  if (response)
    {
      if (self->response_sender(self, response->str, response->len, TRUE) >= 0)
        msg_trace("Sent response", evt_tag_str("http-server-response", response->str));
    }
  if (response)
    g_string_free(response, TRUE);
}

void
log_proto_http_server_init(LogProtoHTTPServer *self, LogTransport *transport,
                           const LogProtoHTTPServerOptionsStorage *options)
{
  log_proto_text_multiline_server_init((LogProtoTextServer *)self, transport, &options->storage);
  self->super.extracted_raw_data_handler = _http_request_handler;
  self->request_processor = _http_request_processor;
  self->request_header_checker = _check_request_headers;
  self->response_header_composer = _compose_response_header;
  self->response_body_composer = _compose_response_body;
  self->response_sender = _send_response;
}

LogProtoServer *
log_proto_http_server_new(LogTransport *transport,
                          const LogProtoServerOptionsStorage *options)
{
  LogProtoHTTPServer *self = g_new0(LogProtoHTTPServer, 1);

  log_proto_http_server_init(self, transport, (LogProtoHTTPServerOptionsStorage *)options);
  return &self->super.super.super;
}
