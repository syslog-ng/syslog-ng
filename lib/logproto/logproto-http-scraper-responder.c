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
#include "logproto/logproto-http-scraper-responder.h"
#include "stats/stats-prometheus.h"
#include "stats/stats-query-commands.h"
#include "messages.h"

static void
_generate_batched_response(const gchar *record, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  //LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)args[0];
  GString **batch = (GString **) args[1];

  g_string_append_printf(*batch, "%s", record);
}

static GString *
_compose_prometheus_response_body(LogProtoHTTPServer *s)
{
  LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)s;

  GString *stats = NULL;
  gboolean cancelled = FALSE;

  if (self->options->stat_type == STT_STAT)
    {
      stats = g_string_new(NULL);
      gpointer args[] = {self, &stats};
      gboolean with_legacy = TRUE;
      stats_generate_prometheus(_generate_batched_response, args, with_legacy, &cancelled);
    }
  else
    stats = stats_execute_query_command("QUERY GET prometheus *", self, &cancelled);

  return stats;
}

static GString *
_compose_response_body(LogProtoHTTPServer *s)
{
  LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)s;
  GString *stats = NULL;

  // Once we have more scraspers to handle these should go into a separate class instead
  switch (self->options->scraper_type)
    {
    case SCT_PROMETHEUS:
      stats = _compose_prometheus_response_body(s);
      break;
    default:
      g_assert(FALSE && "Unknown scraper type");
      break;
    }

  return stats;
}

static gboolean
_check_prometheus_request_headers(LogProtoHTTPServer *s, gchar *buffer_start, gsize buffer_bytes)
{
  // TODO: add a generic header pareser to LogProtoHTTPServer and use it here
  gchar **lines = g_strsplit(buffer_start, "\r\n", 2);

  // First line must be like 'GET /metrics HTTP/1.1\x0d\x0a'
  gchar *line = lines && lines[0] ? lines[0] : buffer_start;
  gchar **tokens = g_strsplit(line, " ", 3);

  gboolean broken = (tokens == NULL || tokens[0] == NULL || strcmp(tokens[0], "GET")
                     || tokens[1] == NULL || strcmp(tokens[1], "/metrics"));

  // TODO: Check further headers as well to support options like compression, etc.
  if (broken)
    msg_error("Unknown request", evt_tag_str("http-scraper-responder", buffer_start));

  if (tokens)
    g_strfreev(tokens);
  if (lines)
    g_strfreev(lines);
  return FALSE == broken;
}

static gboolean
_check_request_headers(LogProtoHTTPServer *s, gchar *buffer_start, gsize buffer_bytes)
{
  LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)s;
  gboolean result = FALSE;

  // Once we have more scraspers to handle these should go into a separate class instead
  switch (self->options->scraper_type)
    {
    case SCT_PROMETHEUS:
      result = _check_prometheus_request_headers(s, buffer_start, buffer_bytes);
      break;
    default:
      g_assert(FALSE && "Unknown scraper type");
      break;
    }
  return result;
}

void
log_proto_http_scraper_responder_server_init(LogProtoHTTPScraperResponder *self, LogTransport *transport,
                                             const LogProtoHTTPScraperResponderOptionsStorage *options)
{
  log_proto_http_server_init((LogProtoHTTPServer *)self, transport,
                             (LogProtoHTTPServerOptionsStorage *)options);

  self->options = (const LogProtoHTTPScraperResponderOptions *)options;
  self->super.request_header_checker = _check_request_headers;
  self->super.response_body_composer = _compose_response_body;
}

LogProtoServer *
log_proto_http_scraper_responder_server_new(LogTransport *transport,
                                            const LogProtoServerOptionsStorage *options)
{
  LogProtoHTTPScraperResponder *self = g_new0(LogProtoHTTPScraperResponder, 1);

  log_proto_http_scraper_responder_server_init(self, transport, (LogProtoHTTPScraperResponderOptionsStorage *)options);
  return &self->super.super.super.super;
}

/* Options */

void
log_proto_http_scraper_responder_options_defaults(LogProtoHTTPScraperResponderOptionsStorage *options)
{
  log_proto_http_server_options_defaults((LogProtoHTTPServerOptionsStorage *)&options->storage);

  options->super.stat_type = STT_STAT;
  options->super.scraper_type = SCT_PROMETHEUS;
}

void
log_proto_http_scraper_responder_options_init(LogProtoHTTPScraperResponderOptionsStorage *options,
                                              GlobalConfig *cfg)
{
  if (options->super.initialized)
    return;

  log_proto_http_server_options_init((LogProtoHTTPServerOptionsStorage *)&options->storage, cfg);

  if (options->super.stat_type == 0)
    options->super.stat_type = STT_STAT;
  if (options->super.scraper_type == 0)
    options->super.scraper_type = SCT_PROMETHEUS;

  options->super.initialized = TRUE;
}

gboolean
log_proto_http_scraper_responder_options_validate(LogProtoHTTPScraperResponderOptionsStorage *options)
{
  if (options->super.stat_type == STT_STAT || options->super.stat_type == STT_QUERY)
    {
      msg_error("prometheus-scraper-response(), type must be 'stat' or 'query'");
      return FALSE;
    }

  return TRUE;
}

void
log_proto_http_scraper_responder_destroy(LogProtoHTTPScraperResponderOptionsStorage *options)
{
  log_proto_http_server_destroy((LogProtoHTTPServerOptionsStorage *)&options->storage);
  options->super.initialized = FALSE;
}
