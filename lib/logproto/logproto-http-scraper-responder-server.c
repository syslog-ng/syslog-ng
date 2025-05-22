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
#include "logproto/logproto-http-scraper-responder-server.h"
#include "stats/stats-prometheus.h"
#include "stats/stats-csv.h"
#include "stats/stats-query-commands.h"
#include "timeutils/cache.h"
#include "messages.h"

#include <iv.h>
#include <glib.h>

static LogProtoHTTPScraperResponder *single_instance;
static time_t last_scrape_request_time;

static inline GMutex *
_mutex(void)
{
  static GMutex *mutex = NULL;

  if (g_once_init_enter(&mutex))
    {
      GMutex *new_mutex = g_new(GMutex, 1);
      g_mutex_init(new_mutex);
      g_once_init_leave(&mutex, new_mutex);
    }
  return mutex;
}

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

  if (self->options->stat_type == STT_STATS)
    {
      stats = g_string_new(NULL);
      gpointer args[] = {self, &stats};
      gboolean with_legacy = TRUE;

      msg_trace("Generating stats", evt_tag_str("stat-format", self->options->stat_format),
                evt_tag_int("with-legacy", with_legacy));
      if (strcmp(self->options->stat_format, "prometheus") == 0)
        stats_generate_prometheus(_generate_batched_response, args, with_legacy, &cancelled);
      else
        {
          gboolean csv = (strcmp(self->options->stat_format, "csv") == 0);
          stats_generate_csv_or_kv(_generate_batched_response, args, csv, FALSE, &cancelled);
        }
    }
  else
    {
      char *query_str = self->options->stat_query && self->options->stat_query[0] ? self->options->stat_query : "*";
      GString *full_query_str = g_string_new(NULL);
      g_string_append_printf(full_query_str, "QUERY GET %s %s", self->options->stat_format, query_str);

      msg_trace("Running stats query", evt_tag_str("stat-format", self->options->stat_format),
                evt_tag_str("stat-query", query_str));
      stats = stats_execute_query_command(full_query_str->str, self, &cancelled);
      g_string_free(full_query_str, TRUE);
    }
  return stats;
}

static GString *
_compose_response_body(LogProtoHTTPServer *s)
{
  LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)s;
  GString *stats = NULL;

  g_mutex_lock(_mutex());
  iv_validate_now();
  time_t now = iv_now.tv_sec;
  time_t ellapsed = now - last_scrape_request_time;
  if (self->options->scrape_freq_limit && ellapsed < self->options->scrape_freq_limit)
    {
      msg_trace("Too frequent scraper requests, ignoring for now",
                evt_tag_int("last-request", ellapsed),
                evt_tag_int("allowed-freq", self->options->scrape_freq_limit));
      g_mutex_unlock(_mutex());
      /* NOTE: Using an empty response body with the close_after_send=TRUE option
       *       prevents the scraper waiting for the response till its own timeout */
      return g_string_new("");
    }
  last_scrape_request_time = now;
  g_mutex_unlock(_mutex());

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
  // TODO: add a generic header parser to LogProtoHTTPServer and use it here
  // First line must be like 'GET /metrics HTTP/1.1\x0d\x0a'
  // We do not care about lines for now, just trying to split the first three items
  // from the beginning (first line) and search for an exact match
  gchar **tokens = g_strsplit(buffer_start, " ", 3);
  gboolean broken = (tokens == NULL || tokens[0] == NULL || strcmp(tokens[0], "GET")
                     || tokens[1] == NULL || strcmp(tokens[1], "/metrics"));

  // TODO: Check further headers as well to support options like compression, etc.
  if (broken)
    msg_error("Unknown request", evt_tag_str("http-scraper-responder", buffer_start));

  if (tokens)
    g_strfreev(tokens);
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

static void
_log_proto_http_scraper_responder_server_free(LogProtoServer *s)
{
  g_mutex_lock(_mutex());
  LogProtoHTTPScraperResponder *self = (LogProtoHTTPScraperResponder *)s;
  if (self->options->single_instance)
    single_instance = NULL;
  g_mutex_unlock(_mutex());
}

void
log_proto_http_scraper_responder_server_init(LogProtoHTTPScraperResponder *self, LogTransport *transport,
                                             const LogProtoServerOptionsStorage *options_storage)
{
  log_proto_http_server_init((LogProtoHTTPServer *)self, transport, options_storage);
  self->super.request_header_checker = _check_request_headers;
  self->super.response_body_composer = _compose_response_body;

  self->super.super.super.super.free_fn = _log_proto_http_scraper_responder_server_free;

  self->options = (const LogProtoHTTPScraperResponderOptions *)options_storage;
}

LogProtoServer *
log_proto_http_scraper_responder_server_new(LogTransport *transport,
                                            const LogProtoServerOptionsStorage *options_storage)
{
  g_mutex_lock(_mutex());

  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;
  if (options->single_instance && single_instance)
    {
      msg_trace("Only one Prometheus scraper responder instance is allowed");
      g_mutex_unlock(_mutex());
      return NULL;
    }

  LogProtoHTTPScraperResponder *self = g_new0(LogProtoHTTPScraperResponder, 1);
  if (options->single_instance)
    single_instance = self;
  log_proto_http_scraper_responder_server_init(self, transport, options_storage);

  g_mutex_unlock(_mutex());
  return &self->super.super.super.super;
}

/*-----------------  Options  -----------------*/

/* NOTE: We do not maintain here the initialized state at all, it is the responsibility of the
 *       LogProtoServer/LogProtoServerOptions functions
 */
void
log_proto_http_scraper_responder_options_defaults(LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  log_proto_http_server_options_defaults(options_storage);
  log_proto_http_server_options_set_close_after_send(options_storage, TRUE);

  options_storage->super.init = log_proto_http_scraper_responder_options_init;
  options_storage->super.validate = log_proto_http_scraper_responder_options_validate;
  options_storage->super.destroy = log_proto_http_scraper_responder_options_destroy;

  options->stat_type = 0;
  options->scraper_type = 0;
  options->scrape_freq_limit = -1;
}

void
log_proto_http_scraper_responder_options_init(LogProtoServerOptionsStorage *options_storage,
                                              GlobalConfig *cfg)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  log_proto_http_server_options_init(options_storage, cfg);
  log_proto_http_server_options_set_close_after_send(options_storage, TRUE);

  if (options->stat_type == 0)
    options->stat_type = STT_STATS;
  if (options->scraper_type == 0)
    options->scraper_type = SCT_PROMETHEUS;
  if (options->scrape_freq_limit == -1)
    options->scrape_freq_limit = 0;
}

void
log_proto_http_scraper_responder_options_destroy(LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  log_proto_http_server_options_destroy(options_storage);
  g_free(options->stat_query);
  options->stat_query = NULL;
  g_free(options->stat_format);
  options->stat_format = NULL;
}

gboolean
log_proto_http_scraper_responder_options_validate(LogProtoServerOptionsStorage *options_storage)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  if (FALSE == log_proto_http_server_options_validate(options_storage))
    return FALSE;

  if (options->stat_type != STT_STATS && options->stat_type != STT_QUERY)
    {
      msg_error("stats-exporter() stat type must be 'stat' or 'query'");
      return FALSE;
    }

  if (options->scraper_type != SCT_PROMETHEUS)
    {
      msg_error("stats-exporter() transport must be 'prometheus'");
      return FALSE;
    }

  return TRUE;
}

gboolean
log_proto_http_scraper_responder_options_set_scrape_type(LogProtoServerOptionsStorage *options_storage,
                                                         const gchar *value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  if (strcmp(value, "prometheus") == 0)
    {
      options->scraper_type = SCT_PROMETHEUS;
      return TRUE;
    }
  return FALSE;
}

void
log_proto_http_scraper_responder_options_set_scrape_freq_limit(
  LogProtoServerOptionsStorage *options_storage,
  gint value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  options->scrape_freq_limit = value;
}

void
log_proto_http_scraper_responder_options_set_single_instance(
  LogProtoServerOptionsStorage *options_storage,
  gboolean value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  options->single_instance = value;
}

gboolean
log_proto_http_scraper_responder_options_set_stat_type(LogProtoServerOptionsStorage *options_storage,
                                                       const gchar *value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  if (strcmp(value, "stats") == 0)
    {
      options->stat_type = STT_STATS;
      return TRUE;
    }
  else if (strcmp(value, "query") == 0)
    {
      options->stat_type = STT_QUERY;
      return TRUE;
    }
  return FALSE;
}

void
log_proto_http_scraper_responder_options_set_stat_query(LogProtoServerOptionsStorage *options_storage,
                                                        const gchar *value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  g_free(options->stat_query);
  options->stat_query = g_strdup(value);
}

gboolean
log_proto_http_scraper_responder_options_set_stat_format(LogProtoServerOptionsStorage *options_storage,
                                                         const gchar *value)
{
  LogProtoHTTPScraperResponderOptions *options = &((LogProtoHTTPScraperResponderOptionsStorage *)options_storage)->super;

  if (strcmp(value, "prometheus") == 0 || strcmp(value, "csv") == 0 || strcmp(value, "kv") == 0)
    {
      g_free(options->stat_format);
      options->stat_format = g_strdup(value);
      return TRUE;
    }
  return FALSE;
}
