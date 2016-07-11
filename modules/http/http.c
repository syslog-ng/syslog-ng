/*
 * Copyright (c) 2016 Marc Falzon
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <curl/curl.h>

#include "syslog-names.h"
#include "http-plugin.h"

static const gchar *
_format_persist_name(const LogPipe *s)
{
  const HTTPDestinationDriver *self = (const HTTPDestinationDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "http.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "http(%s,)", self->url);

  return persist_name;
}

static gchar *
_format_stats_instance(LogThrDestDriver *s)
{
  static gchar stats[1024];

  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;

  g_snprintf(stats, sizeof(stats), "http,%s", self->url);

  return stats;
}

static size_t
_http_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // Discard response content
    return nmemb * size;
}

static void
_thread_init(LogThrDestDriver *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;

  curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
  if (!self->user_agent)
    self->user_agent = g_strdup_printf("syslog-ng %s/libcurl %s",
                                        SYSLOG_NG_VERSION, curl_info->version);
}

static void
_thread_deinit(LogThrDestDriver *s)
{
}

static gboolean
_connect(LogThrDestDriver *s)
{
  return TRUE;
}

static void
_disconnect(LogThrDestDriver *s)
{
}

static struct curl_slist *
_get_curl_headers(HTTPDestinationDriver *self, LogMessage *msg)
{
  GList *header = NULL;
  struct curl_slist *curl_headers = NULL;
  gchar header_host[128] = {0};
  gchar header_program[32] = {0};
  gchar header_facility[32] = {0};
  gchar header_level[32] = {0};

  g_snprintf(header_host, sizeof(header_host),
    "X-Syslog-Host: %s", log_msg_get_value(msg, LM_V_HOST, NULL));
  curl_headers = curl_slist_append(curl_headers, header_host);

  g_snprintf(header_program, sizeof(header_program),
    "X-Syslog-Program: %s", log_msg_get_value(msg, LM_V_PROGRAM, NULL));
  curl_headers = curl_slist_append(curl_headers, header_program);

  g_snprintf(header_facility, sizeof(header_facility),
    "X-Syslog-Facility: %s", syslog_name_lookup_name_by_value(msg->pri & LOG_FACMASK, sl_facilities));
  curl_headers = curl_slist_append(curl_headers, header_facility);

  g_snprintf(header_level, sizeof(header_level),
    "X-Syslog-Level: %s", syslog_name_lookup_name_by_value(msg->pri & LOG_PRIMASK, sl_levels));
  curl_headers = curl_slist_append(curl_headers, header_level);

  header = self->headers;
  while (header != NULL) {
    curl_headers = curl_slist_append(curl_headers, (gchar *)header->data);
    header = g_list_next(header);
  }

  return curl_headers;
}

static GString *
_get_body_rendered(HTTPDestinationDriver *self, LogMessage *msg)
{
  GString *body_rendered = NULL;

  if (self->body_template) {
    body_rendered = g_string_new(NULL);
    log_template_format(self->body_template, msg, &self->template_options, LTZ_SEND,
                        self->super.seq_num, NULL, body_rendered);
  }
  return body_rendered;
}

static void
_set_curl_opt(HTTPDestinationDriver *self, LogMessage *msg, struct curl_slist *curl_headers, GString *body_rendered)
{
  curl_easy_reset(self->curl);

  curl_easy_setopt(self->curl, CURLOPT_WRITEFUNCTION, _http_write_cb);

  curl_easy_setopt(self->curl, CURLOPT_URL, self->url);

  if (self->user)
    curl_easy_setopt(self->curl, CURLOPT_USERNAME, self->user);

  if (self->password)
    curl_easy_setopt(self->curl, CURLOPT_PASSWORD, self->password);

  if (self->user_agent)
    curl_easy_setopt(self->curl, CURLOPT_USERAGENT, self->user_agent);


  curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, curl_headers);

  const gchar *body = body_rendered ? body_rendered->str : log_msg_get_value(msg, LM_V_MESSAGE, NULL);

  curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, body);
  if (self->method_type == METHOD_TYPE_PUT)
    curl_easy_setopt(self->curl, CURLOPT_CUSTOMREQUEST, "PUT");
}

static worker_insert_result_t
_insert(LogThrDestDriver *s, LogMessage *msg)
{
  CURLcode ret;

  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;

  struct curl_slist * curl_headers = _get_curl_headers(self, msg);
  GString *body_rendered = _get_body_rendered(self, msg);

  _set_curl_opt(self, msg, curl_headers, body_rendered);

  if ((ret = curl_easy_perform(self->curl)) != CURLE_OK) {
      msg_error("curl: error sending HTTP request",
                evt_tag_str("error", curl_easy_strerror(ret)));

      if (body_rendered)
        g_string_free(body_rendered, TRUE);

      curl_slist_free_all(curl_headers);

      return WORKER_INSERT_RESULT_ERROR;
  }

  if (body_rendered)
    g_string_free(body_rendered, TRUE);

  curl_slist_free_all(curl_headers);

  return WORKER_INSERT_RESULT_SUCCESS;
}

void
http_dd_set_url(LogDriver *d, const gchar *url)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->url);
  self->url = g_strdup(url);
}

void
http_dd_set_user(LogDriver *d, const gchar *user)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->user);
  self->user = g_strdup(user);
}

void
http_dd_set_password(LogDriver *d, const gchar *password)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->password);
  self->password = g_strdup(password);
}

void
http_dd_set_user_agent(LogDriver *d, const gchar *user_agent)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->user_agent);
  self->user_agent = g_strdup(user_agent);
}

void
http_dd_set_headers(LogDriver *d, GList *headers)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_list_free_full(self->headers, g_free);
  self->headers = g_list_copy(headers);

  GList *header = self->headers;
  while (header != NULL) {
    header->data = g_strdup(header->data);
    header = g_list_next(header);
  }
}

void
http_dd_set_method(LogDriver *d, const gchar *method)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  if (g_ascii_strcasecmp(method, "POST") == 0)
    self->method_type = METHOD_TYPE_POST;
  else if (g_ascii_strcasecmp(method, "PUT") == 0)
    self->method_type = METHOD_TYPE_PUT;
  else
    {
      msg_warning("Unsupported method is set(Only POST and PUT are supported), default method POST will be used",
                  evt_tag_str("method", method));
      self->method_type = METHOD_TYPE_POST;
    }
}

void
http_dd_set_body(LogDriver *d, LogTemplate *body)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  log_template_unref(self->body_template);
  self->body_template = log_template_ref(body);
}

LogTemplateOptions *
http_dd_get_template_options(LogDriver *d)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  return &self->template_options;
}

gboolean
http_dd_init(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  if (!self->url) {
    self->url = g_strdup(HTTP_DEFAULT_URL);
  }

  return log_threaded_dest_driver_start(s);
}

gboolean
http_dd_deinit(LogPipe *s)
{
  return log_threaded_dest_driver_deinit_method(s);
}

static void
http_dd_free(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;

  curl_easy_cleanup(self->curl);
  curl_global_cleanup();

  g_free(self->url);
  g_free(self->user);
  g_free(self->password);
  g_free(self->user_agent);
  g_list_free_full(self->headers, g_free);

  log_threaded_dest_driver_free(s);
}


LogDriver *
http_dd_new(GlobalConfig *cfg)
{
  HTTPDestinationDriver *self = g_new0(HTTPDestinationDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = http_dd_init;
  self->super.super.super.super.deinit = http_dd_deinit;
  self->super.worker.thread_init = _thread_init;
  self->super.worker.thread_deinit = _thread_deinit;
  self->super.worker.connect = _connect;
  self->super.worker.disconnect = _disconnect;
  self->super.worker.insert = _insert;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.format.stats_instance = _format_stats_instance;
  self->super.stats_source = SCS_HTTP;
  self->super.super.super.super.free_fn = http_dd_free;

  curl_global_init(CURL_GLOBAL_ALL);

  if (!(self->curl = curl_easy_init())) {
    msg_error("curl: cannot initialize libcurl", NULL);

    return NULL;
  }

  return &self->super.super.super;
}
