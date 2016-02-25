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
#include "curl-plugin.h"

static gchar *
_format_persist_name(LogThrDestDriver *s)
{
  static gchar persist_name[1024];

  CurlDestinationDriver *self = (CurlDestinationDriver *) s;

  g_snprintf(persist_name, sizeof(persist_name), "curl(%s,)", self->url);

  return persist_name;
}

static gchar *
_format_stats_instance(LogThrDestDriver *s)
{
  static gchar stats[1024];

  CurlDestinationDriver *self = (CurlDestinationDriver *) s;

  g_snprintf(stats, sizeof(stats), "curl,%s", self->url);

  return stats;
}

static size_t
_curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // Discard response content
    return nmemb * size;
}

static void
_thread_init(LogThrDestDriver *s)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) s;

  curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
  g_snprintf(self->user_agent, sizeof(self->user_agent),
             "syslog-ng %s/libcurl %s", SYSLOG_NG_VERSION, curl_info->version);

  if (!self->url) {
    self->url = g_strdup(CURL_DEFAULT_URL);
  }
}

static void
_thread_deinit(LogThrDestDriver *s)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) s;

  curl_easy_cleanup(self->curl);
  curl_global_cleanup();

  g_free(self->url);
  g_free(self->user);
  g_free(self->password);
  g_list_free(self->headers);
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

static worker_insert_result_t
_insert(LogThrDestDriver *s, LogMessage *msg)
{
  const gchar *log_host = NULL;
  const gchar *log_program = NULL;
  const gchar *log_facility = NULL;
  const gchar *log_level = NULL;
  const gchar *body = NULL;
  GString *body_rendered = NULL;
  GList *header = NULL;
  gchar header_host[128] = {0};
  gchar header_program[32] = {0};
  gchar header_facility[32] = {0};
  gchar header_level[32] = {0};
  struct curl_slist *curl_headers = NULL;
  CURLcode ret;

  CurlDestinationDriver *self = (CurlDestinationDriver *) s;

  log_host = log_msg_get_value(msg, LM_V_HOST, NULL);
  log_program = log_msg_get_value(msg, LM_V_PROGRAM, NULL);
  log_facility = syslog_name_lookup_name_by_value(msg->pri & LOG_FACMASK, sl_facilities);
  log_level = syslog_name_lookup_name_by_value(msg->pri & LOG_PRIMASK, sl_levels);

  if (self->body_template) {
    body_rendered = g_string_new(NULL);
    log_template_format(self->body_template, msg, &self->template_options, LTZ_SEND,
                        self->super.seq_num, NULL, body_rendered);
    body = body_rendered->str;
  } else
    body = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

  curl_easy_reset(self->curl);

  curl_easy_setopt(self->curl, CURLOPT_WRITEFUNCTION, _curl_write_cb);

  curl_easy_setopt(self->curl, CURLOPT_URL, self->url);

  if (self->user)
    curl_easy_setopt(self->curl, CURLOPT_USERNAME, self->user);

  if (self->password)
    curl_easy_setopt(self->curl, CURLOPT_PASSWORD, self->password);

  curl_easy_setopt(self->curl, CURLOPT_USERAGENT, self->user_agent);

  g_snprintf(header_host, sizeof(header_host),
    "X-Syslog-Host: %s", log_host);
  curl_headers = curl_slist_append(curl_headers, header_host);

  g_snprintf(header_program, sizeof(header_program),
    "X-Syslog-Program: %s", log_program);
  curl_headers = curl_slist_append(curl_headers, header_program);

  g_snprintf(header_facility, sizeof(header_facility),
    "X-Syslog-Facility: %s", log_facility);
  curl_headers = curl_slist_append(curl_headers, header_facility);

  g_snprintf(header_level, sizeof(header_level),
    "X-Syslog-Level: %s", log_level);
  curl_headers = curl_slist_append(curl_headers, header_level);

  header = self->headers;
  while (header != NULL) {
    curl_headers = curl_slist_append(curl_headers, (gchar *)header->data);
    header = g_list_next(header);
  }

  curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, curl_headers);

  curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, body);

  if ((ret = curl_easy_perform(self->curl)) != CURLE_OK) {
      msg_error("curl: error sending HTTP request",
      evt_tag_str("error", curl_easy_strerror(ret)), NULL);

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
curl_dd_set_url(LogDriver *d, const gchar *url)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  g_free(self->url);
  self->url = g_strdup(url);
}

void
curl_dd_set_user(LogDriver *d, const gchar *user)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  g_free(self->user);
  self->user = g_strdup(user);
}

void
curl_dd_set_password(LogDriver *d, const gchar *password)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  g_free(self->password);
  self->password = g_strdup(password);
}

void
curl_dd_set_headers(LogDriver *d, GList *headers)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  g_list_free(self->headers);
  self->headers = headers;
}

void
curl_dd_set_body(LogDriver *d, LogTemplate *body)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  log_template_unref(self->body_template);
  self->body_template = log_template_ref(body);
}

LogTemplateOptions *
curl_dd_get_template_options(LogDriver *d)
{
  CurlDestinationDriver *self = (CurlDestinationDriver *) d;

  return &self->template_options;
}

LogDriver *
curl_dd_new(GlobalConfig *cfg)
{
  CurlDestinationDriver *self = g_new0(CurlDestinationDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.worker.thread_init = _thread_init;
  self->super.worker.thread_deinit = _thread_deinit;
  self->super.worker.connect = _connect;
  self->super.worker.disconnect = _disconnect;
  self->super.worker.insert = _insert;
  self->super.format.persist_name = _format_persist_name;
  self->super.format.stats_instance = _format_stats_instance;
  self->super.stats_source = SCS_CURL;

  curl_global_init(CURL_GLOBAL_ALL);

  if (!(self->curl = curl_easy_init())) {
    msg_error("curl: cannot initialize libcurl", NULL, NULL);

    return NULL;
  }

  return &self->super.super.super;
}
