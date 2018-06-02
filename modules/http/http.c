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

#include "http.h"
#include "syslog-names.h"
#include "scratch-buffers.h"

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
  self->headers = g_list_copy_deep(headers, ((GCopyFunc)g_strdup), NULL);
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

void
http_dd_set_ca_dir(LogDriver *d, const gchar *ca_dir)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->ca_dir);
  self->ca_dir = g_strdup(ca_dir);
}

void
http_dd_set_ca_file(LogDriver *d, const gchar *ca_file)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->ca_file);
  self->ca_file = g_strdup(ca_file);
}

void
http_dd_set_cert_file(LogDriver *d, const gchar *cert_file)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->cert_file);
  self->cert_file = g_strdup(cert_file);
}

void
http_dd_set_key_file(LogDriver *d, const gchar *key_file)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->key_file);
  self->key_file = g_strdup(key_file);
}

void
http_dd_set_cipher_suite(LogDriver *d, const gchar *ciphers)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_free(self->ciphers);
  self->ciphers = g_strdup(ciphers);
}

void
http_dd_set_ssl_version(LogDriver *d, const gchar *value)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  if (strcmp(value, "default") == 0)
    {
      /*
       * Negotiate the version based on what the remote server supports.
       * SSLv2 is disabled by default as of libcurl 7.18.1.
       * SSLv3 is disabled by default as of libcurl 7.39.0.
       */
      self->ssl_version = CURL_SSLVERSION_DEFAULT;

    }
  else if (strcmp(value, "tlsv1") == 0)
    {
      /* TLS 1.x */
      self->ssl_version = CURL_SSLVERSION_TLSv1;
    }
  else if (strcmp(value, "sslv2") == 0)
    {
      /* SSL 2 only */
      self->ssl_version = CURL_SSLVERSION_SSLv2;

    }
  else if (strcmp(value, "sslv3") == 0)
    {
      /* SSL 3 only */
      self->ssl_version = CURL_SSLVERSION_SSLv3;
    }
#ifdef CURL_SSLVERSION_TLSv1_0
  else if (strcmp(value, "tlsv1_0") == 0)
    {
      /* TLS 1.0 only */
      self->ssl_version = CURL_SSLVERSION_TLSv1_0;
    }
#endif
#ifdef CURL_SSLVERSION_TLSv1_1
  else if (strcmp(value, "tlsv1_1") == 0)
    {
      /* TLS 1.1 only */
      self->ssl_version = CURL_SSLVERSION_TLSv1_1;
    }
#endif
#ifdef CURL_SSLVERSION_TLSv1_2
  else if (strcmp(value, "tlsv1_2") == 0)
    {
      /* TLS 1.2 only */
      self->ssl_version = CURL_SSLVERSION_TLSv1_2;
    }
#endif
  else
    {
      msg_warning("curl: unsupported SSL version",
                  evt_tag_str("ssl_version", value));
    }
}

void
http_dd_set_peer_verify(LogDriver *d, gboolean verify)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->peer_verify = verify;
}

void
http_dd_set_timeout(LogDriver *d, glong timeout)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->timeout = timeout;
}

void
http_dd_set_flush_lines(LogDriver *d, glong flush_lines)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->flush_lines = flush_lines;
}

void
http_dd_set_flush_bytes(LogDriver *d, glong flush_bytes)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->flush_bytes = flush_bytes;
}


static gchar *
_sanitize_curl_debug_message(const gchar *data, gsize size)
{
  gchar *sanitized = g_new0(gchar, size + 1);
  gint i;

  for (i = 0; i < size && data[i]; i++)
    {
      sanitized[i] = g_ascii_isprint(data[i]) ? data[i] : '.';
    }
  sanitized[i] = 0;
  return sanitized;
}

static const gchar *curl_infotype_to_text[] =
{
  "text",
  "header_in",
  "header_out",
  "data_in",
  "data_out",
  "ssl_data_in",
  "ssl_data_out",
};

static gint
_curl_debug_function(CURL *handle, curl_infotype type,
                     char *data, size_t size,
                     void *userp)
{
  if (!G_UNLIKELY(trace_flag))
    return 0;

  g_assert(type < sizeof(curl_infotype_to_text)/sizeof(curl_infotype_to_text[0]));

  const gchar *text = curl_infotype_to_text[type];
  gchar *sanitized = _sanitize_curl_debug_message(data, size);
  msg_trace("cURL debug",
            evt_tag_str("type", text),
            evt_tag_str("data", sanitized));
  g_free(sanitized);
  return 0;
}

static size_t
_curl_write_function(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  // Discard response content
  return nmemb * size;
}

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

static const gchar *
_format_stats_instance(LogThreadedDestDriver *s)
{
  static gchar stats[1024];

  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;

  g_snprintf(stats, sizeof(stats), "http,%s", self->url);

  return stats;
}


/* Set up options that are static over the course of a single configuration,
 * request specific options will be set separately
 */
static void
_setup_static_options_in_curl(HTTPDestinationDriver *self)
{
  curl_easy_reset(self->curl);

  curl_easy_setopt(self->curl, CURLOPT_WRITEFUNCTION, _curl_write_function);

  curl_easy_setopt(self->curl, CURLOPT_URL, self->url);

  if (self->user)
    curl_easy_setopt(self->curl, CURLOPT_USERNAME, self->user);

  if (self->password)
    curl_easy_setopt(self->curl, CURLOPT_PASSWORD, self->password);

  if (self->user_agent)
    curl_easy_setopt(self->curl, CURLOPT_USERAGENT, self->user_agent);

  if (self->ca_dir)
    curl_easy_setopt(self->curl, CURLOPT_CAPATH, self->ca_dir);

  if (self->ca_file)
    curl_easy_setopt(self->curl, CURLOPT_CAINFO, self->ca_file);

  if (self->cert_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLCERT, self->cert_file);

  if (self->key_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLKEY, self->key_file);

  if (self->ciphers)
    curl_easy_setopt(self->curl, CURLOPT_SSL_CIPHER_LIST, self->ciphers);

  curl_easy_setopt(self->curl, CURLOPT_SSLVERSION, self->ssl_version);
  curl_easy_setopt(self->curl, CURLOPT_SSL_VERIFYHOST, self->peer_verify ? 2L : 0L);
  curl_easy_setopt(self->curl, CURLOPT_SSL_VERIFYPEER, self->peer_verify ? 1L : 0L);

  curl_easy_setopt(self->curl, CURLOPT_DEBUGFUNCTION, _curl_debug_function);
  curl_easy_setopt(self->curl, CURLOPT_VERBOSE, (long) (debug_flag || trace_flag));

  curl_easy_setopt(self->curl, CURLOPT_TIMEOUT, self->timeout);

  if (self->method_type == METHOD_TYPE_PUT)
    curl_easy_setopt(self->curl, CURLOPT_CUSTOMREQUEST, "PUT");
}


static struct curl_slist *
_add_header(struct curl_slist *curl_headers, const gchar *header, const gchar *value)
{
  GString *buffer = scratch_buffers_alloc();

  g_string_append(buffer, header);
  g_string_append(buffer, ": ");
  g_string_append(buffer, value);
  return curl_slist_append(curl_headers, buffer->str);
}

static struct curl_slist *
_format_request_headers(HTTPDestinationDriver *self, LogMessage *msg)
{
  struct curl_slist *headers = NULL;
  GList *l;

  headers = _add_header(headers, "Expect", "");
  if (msg)
    {
      /* NOTE: I have my doubts that these headers make sense at all.  None of
       * the HTTP collectors I know of, extract this information from the
       * headers and it makes batching several messages into the same request a
       * bit more complicated than it needs to be.  I didn't want to break
       * backward compatibility when batching was introduced, however I think
       * this should eventually be removed */

      headers = _add_header(headers,
                            "X-Syslog-Host",
                            log_msg_get_value(msg, LM_V_HOST, NULL));
      headers = _add_header(headers,
                            "X-Syslog-Program",
                            log_msg_get_value(msg, LM_V_PROGRAM, NULL));
      headers = _add_header(headers,
                            "X-Syslog-Facility",
                            syslog_name_lookup_name_by_value(msg->pri & LOG_FACMASK, sl_facilities));
      headers = _add_header(headers,
                            "X-Syslog-Level",
                            syslog_name_lookup_name_by_value(msg->pri & LOG_PRIMASK, sl_levels));
    }

  for (l = self->headers; l; l = l->next)
    headers = curl_slist_append(headers, l->data);

  return headers;
}

static void
_add_message_to_batch(HTTPDestinationDriver *self, LogMessage *msg)
{
  if (self->body_template)
    {
      log_template_append_format(self->body_template, msg, &self->template_options, LTZ_SEND,
                                 self->super.seq_num, NULL, self->request_body);
    }
  else
    {
      if (self->request_body->len)
        g_string_append_c(self->request_body, '\n');
      g_string_append(self->request_body, log_msg_get_value(msg, LM_V_MESSAGE, NULL));
    }
}

static worker_insert_result_t
_map_http_status_to_worker_status(glong http_code)
{
  worker_insert_result_t retval;

  switch (http_code/100)
    {
    case 4:
      retval = WORKER_INSERT_RESULT_DROP;
      break;
    case 5:
      retval = WORKER_INSERT_RESULT_ERROR;
      break;
    default:
      retval = WORKER_INSERT_RESULT_SUCCESS;
      break;
    }

  return retval;
}

/* we flush the accumulated data if
 *   1) we reach batch_size,
 *   2) the message queue becomes empty
 */
static worker_insert_result_t
_flush(LogThreadedDestDriver *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;
  CURLcode ret;
  worker_insert_result_t retval;

  if (self->super.batch_size == 0)
    return WORKER_INSERT_RESULT_SUCCESS;

  curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, self->request_headers);
  curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->request_body->str);

  if ((ret = curl_easy_perform(self->curl)) != CURLE_OK)
    {
      msg_error("curl: error sending HTTP request",
                evt_tag_str("error", curl_easy_strerror(ret)),
                log_pipe_location_tag(&self->super.super.super.super));
      retval = WORKER_INSERT_RESULT_NOT_CONNECTED;
      goto exit;
    }

  glong http_code = 0;

  curl_easy_getinfo(self->curl, CURLINFO_RESPONSE_CODE, &http_code);

  if (debug_flag)
    {
      gdouble total_time = 0;
      glong redirect_count = 0;

      curl_easy_getinfo(self->curl, CURLINFO_TOTAL_TIME, &total_time);
      curl_easy_getinfo(self->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);
      msg_debug("curl: HTTP response received",
                evt_tag_str("url", self->url),
                evt_tag_int("status_code", http_code),
                evt_tag_int("body_size", self->request_body->len),
                evt_tag_int("batch_size", self->super.batch_size),
                evt_tag_int("redirected", redirect_count != 0),
                evt_tag_printf("total_time", "%.3f", total_time));
    }
  retval = _map_http_status_to_worker_status(http_code);

exit:
  g_string_truncate(self->request_body, 0);
  curl_slist_free_all(self->request_headers);
  self->request_headers = NULL;
  return retval;
}

static worker_insert_result_t
_insert_batched(HTTPDestinationDriver *self, LogMessage *msg)
{
  if (self->request_headers == NULL)
    self->request_headers = _format_request_headers(self, NULL);

  _add_message_to_batch(self, msg);

  if ((self->flush_bytes && self->request_body->len >= self->flush_bytes) ||
      (self->flush_lines && self->super.batch_size >= self->flush_lines))
    {
      return _flush(&self->super);
    }
  return WORKER_INSERT_RESULT_QUEUED;
}

static worker_insert_result_t
_insert_single(HTTPDestinationDriver *self, LogMessage *msg)
{
  self->request_headers = _format_request_headers(self, msg);
  _add_message_to_batch(self, msg);
  return _flush(&self->super);
}

static worker_insert_result_t
_insert(LogThreadedDestDriver *s, LogMessage *msg)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) s;

  if (self->flush_lines > 0)
    return _insert_batched(self, msg);
  else
    return _insert_single(self, msg);
}

gboolean
http_dd_init(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  if (!(self->curl = curl_easy_init()))
    {
      msg_error("curl: cannot initialize libcurl",
                log_pipe_location_tag(s));
      return FALSE;
    }

  if (!self->url)
    {
      self->url = g_strdup(HTTP_DEFAULT_URL);
    }

  curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
  if (!self->user_agent)
    self->user_agent = g_strdup_printf("syslog-ng %s/libcurl %s",
                                       SYSLOG_NG_VERSION, curl_info->version);

  _setup_static_options_in_curl(self);

  return log_threaded_dest_driver_init_method(s);
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

  log_template_options_destroy(&self->template_options);

  g_string_free(self->request_body, TRUE);

  curl_easy_cleanup(self->curl);
  curl_global_cleanup();

  g_free(self->url);
  g_free(self->user);
  g_free(self->password);
  g_free(self->user_agent);
  g_free(self->ca_dir);
  g_free(self->ca_file);
  g_free(self->cert_file);
  g_free(self->key_file);
  g_free(self->ciphers);
  g_list_free_full(self->headers, g_free);

  log_threaded_dest_driver_free(s);
}

LogDriver *
http_dd_new(GlobalConfig *cfg)
{
  HTTPDestinationDriver *self = g_new0(HTTPDestinationDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = http_dd_init;
  self->super.super.super.super.free_fn = http_dd_free;
  self->super.super.super.super.deinit = http_dd_deinit;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.format_stats_instance = _format_stats_instance;
  self->super.stats_source = SCS_HTTP;
  self->super.worker.insert = _insert;
  self->super.worker.flush = _flush;

  curl_global_init(CURL_GLOBAL_ALL);

  self->ssl_version = CURL_SSLVERSION_DEFAULT;
  self->peer_verify = TRUE;
  self->request_body = g_string_sized_new(32768);
  self->flush_lines = 0;
  self->flush_bytes = 0;

  return &self->super.super.super;
}
