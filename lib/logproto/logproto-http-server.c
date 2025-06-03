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
#include "logproto/logproto-http-server.h"
#include "messages.h"

#include <unistd.h>

static GString *
_compose_response_body(LogProtoHTTPServer *self)
{
  return NULL;
}

static gint
_check_request_headers(LogProtoHTTPServer *self, gchar *buffer_start, gsize buffer_bytes)
{
  return 400; // HTTP/1.1 400 Bad Request
}

static GString *
_http_request_processor(LogProtoHTTPServer *self, LogProtoBufferedServerState *state,
                        const guchar *buffer_start, gsize buffer_bytes)
{
  GString *response_data = NULL;
  gint status = self->request_header_checker(self, (gchar *)buffer_start, buffer_bytes);

  switch (status)
    {
    case 200:
      response_data = self->response_body_composer(self);
      break;

    case 429: // HTTP/1.1 429 Too Many Requests
      msg_trace("http-server(): Too many requests");
      response_data = g_string_new(http_too_many_request_msg);
      g_string_append(response_data, "\n\n");
      break;

    case 400: // HTTP/1.1 400 Bad Request
    default:
    {
      GString *header = g_string_new_len((gchar *)buffer_start, buffer_bytes);
      msg_trace("http-server(): Bad request",
                evt_tag_str("header", header->str));
      response_data = g_string_new(http_bad_request_msg);
      g_string_append(response_data, "\n\n");
      g_string_free(header, TRUE);
      break;
    }
    }
  return response_data;
}

static GString *
_compose_response_header(LogProtoHTTPServer *self, const gchar *data G_GNUC_UNUSED, gsize data_len,
                         gboolean close_after_sent)
{
  const gint maxContentNumLength = 5;
  static const gchar close_str[] = "Connection: close\n";
  static const gchar response_header_fmt[] = "%s\n"
                                             "Content-Type: text/plain; version=0.0.4\n"
                                             "Content-Length: %*lu\n"
                                             "%s\n"
                                             "\n";
  GString *response = g_string_sized_new(G_N_ELEMENTS(response_header_fmt) - 1 +
                                         G_N_ELEMENTS(http_ok_msg) - 1 +
                                         G_N_ELEMENTS(close_str) - 1 - 2 +
                                         -4 + maxContentNumLength + data_len);
  g_string_printf(response, response_header_fmt, http_ok_msg, maxContentNumLength, data_len,
                  close_after_sent ? close_str : "");
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
    sent_bytes = log_transport_stack_write(&self->super.super.super.transport_stack, response->str, response->len);

  if (response)
    g_string_free(response, TRUE);
  return sent_bytes;
}

static gboolean
_http_request_handler(LogProtoTextServer *s, LogProtoBufferedServerState *state,
                      const guchar *buffer_start, gsize buffer_bytes)
{
  gboolean result = FALSE;
  LogProtoHTTPServer *self = (LogProtoHTTPServer *)s;

  GString *response = self->request_processor(self, state, buffer_start, buffer_bytes);
  if (response)
    {
      result = response->len > 0;
      if (self->response_sender(self, response->str, response->len, self->options->super.close_after_send) >= 0)
        msg_trace("http-server(): Sent response", evt_tag_str("http-server-response", response->str));
    }
  if (response)
    g_string_free(response, TRUE);
  return result;
}

void
log_proto_http_server_init(LogProtoHTTPServer *self, LogTransport *transport,
                           const LogProtoServerOptionsStorage *options_storage)
{
  log_proto_text_multiline_server_init((LogProtoTextServer *)self, transport, options_storage);

  self->options = (const LogProtoHTTPServerOptionsStorage *)options_storage;

  self->super.extracted_raw_data_handler = _http_request_handler;
  self->request_processor = _http_request_processor;
  self->request_header_checker = _check_request_headers;
  self->response_header_composer = _compose_response_header;
  self->response_body_composer = _compose_response_body;
  self->response_sender = _send_response;
}

LogProtoServer *
log_proto_http_server_new(LogTransport *transport,
                          const LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPServer *self = g_new0(LogProtoHTTPServer, 1);

  log_proto_http_server_init(self, transport, options_storage);
  return &self->super.super.super;
}

/*-----------------  Options  -----------------*/

/* NOTE: We do not maintain here the initialized state at all, it is the responsibility of the
 *       LogProtoServer/LogProtoServerOptions functions
 */
void
log_proto_http_server_options_defaults(LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPServerOptions *options = &((LogProtoHTTPServerOptionsStorage *)options_storage)->super;

  options->super.init = log_proto_http_server_options_init;
  options->super.validate = log_proto_http_server_options_validate;
  options->super.destroy = log_proto_http_server_options_destroy;

  multi_line_options_set_keep_trailing_newline(&options->super.multi_line_options, TRUE);
  options->close_after_send = FALSE;
}

void
log_proto_http_server_options_init(LogProtoServerOptionsStorage *options_storage,
                                   GlobalConfig *cfg)
{
  LogProtoHTTPServerOptions *options = &((LogProtoHTTPServerOptionsStorage *)options_storage)->super;
  multi_line_options_set_mode(&options->super.multi_line_options, "empty-line-separated");
  multi_line_options_set_keep_trailing_newline(&options->super.multi_line_options, TRUE);
  options->close_after_send = FALSE;
}

void
log_proto_http_server_options_destroy(LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPServerOptions *options = &((LogProtoHTTPServerOptionsStorage *)options_storage)->super;

  options->super.init = NULL;
  options->super.validate = NULL;
  options->super.destroy = NULL;
}

gboolean
log_proto_http_server_options_validate(LogProtoServerOptionsStorage *options_storage)
{
  return TRUE;
}

void
log_proto_http_server_options_set_close_after_send(LogProtoServerOptionsStorage *options_storage,
                                                   gboolean value)
{
  LogProtoHTTPServerOptions *options = &((LogProtoHTTPServerOptionsStorage *)options_storage)->super;
  options->close_after_send = value;
}
