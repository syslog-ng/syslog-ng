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
#include "http-worker.h"

/* HTTPDestinationDriver */

void
http_dd_set_urls(LogDriver *d, GList *urls)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  http_load_balancer_drop_all_targets(self->load_balancer);
  for (GList *l = urls; l; l = l->next)
    http_load_balancer_add_target(self->load_balancer, l->data);
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
http_dd_set_auth_header(LogDriver *d, HttpAuthHeader *auth_header)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->auth_header = auth_header;
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

void
http_dd_set_delimiter(LogDriver *d, const gchar *delimiter)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_string_assign(self->delimiter, delimiter);
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
http_dd_set_use_system_cert_store(LogDriver *d, gboolean enable)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->use_system_cert_store = enable;
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
http_dd_set_accept_redirects(LogDriver *d, gboolean accept_redirects)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->accept_redirects = accept_redirects;
}

void
http_dd_set_timeout(LogDriver *d, glong timeout)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->timeout = timeout;
}

void
http_dd_set_batch_bytes(LogDriver *d, glong batch_bytes)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  self->batch_bytes = batch_bytes;
}

void
http_dd_set_body_prefix(LogDriver *d, const gchar *body_prefix)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_string_assign(self->body_prefix, body_prefix);
}

void
http_dd_set_body_suffix(LogDriver *d, const gchar *body_suffix)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *) d;

  g_string_assign(self->body_suffix, body_suffix);
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

static const gchar *
_format_auth_header_name(const LogPipe *s)
{
  static gchar auth_header_name[1024];
  const gchar *persist_name = _format_persist_name(s);
  g_snprintf(auth_header_name, sizeof(auth_header_name), "%s.auth_header", persist_name);

  return auth_header_name;
}

static gboolean
_load_auth_header_from_persist_file(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  gsize size;
  guint8 version;
  const gchar *persist_name = _format_auth_header_name(s);
  gchar *auth_header_str = persist_state_lookup_string(cfg->state, persist_name, &size, &version);

  if (!auth_header_str)
    return FALSE;

  gboolean ret = http_auth_header_load_from_string(self->auth_header, auth_header_str);
  g_free(auth_header_str);

  return ret;
}

static void
_save_auth_header_to_persist_file(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  const gchar *persist_name = _format_auth_header_name(s);
  const gchar *auth_header_str = http_auth_header_get_as_string(self->auth_header);

  persist_state_alloc_string(cfg->state, persist_name, auth_header_str, -1);
}

static gboolean
_auth_header_renew(HTTPDestinationDriver *self)
{
  gboolean ret = TRUE;

  if (http_auth_header_has_expired(self->auth_header))
    {
      ret = http_auth_header_renew(self->auth_header);
      if (ret)
        {
          _save_auth_header_to_persist_file(&self->super.super.super.super);
        }
    }

  return ret;
}

static void
_load_auth_header(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;

  if (_load_auth_header_from_persist_file(s))
    return;

  if (!_auth_header_renew(self))
    {
      msg_warning("WARNING: http() driver failed to get auth header",
                  log_pipe_location_tag(s));
    }
}

gboolean
http_dd_auth_header_renew(LogDriver *d)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)d;

  gboolean ret = TRUE;
  g_mutex_lock(self->workers_lock);
  {
    ret = _auth_header_renew(self);
  }
  g_mutex_unlock(self->workers_lock);

  return ret;
}

gboolean
http_dd_init(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->load_balancer->num_targets == 0)
    http_load_balancer_add_target(self->load_balancer, HTTP_DEFAULT_URL);

  if (self->load_balancer->num_targets > 1 && s->persist_name == NULL)
    {
      msg_warning("WARNING: your http() driver instance uses multiple urls without persist-name(). "
                  "It is recommended that you set persist-name() in this case as syslog-ng will be "
                  "using the first URL in urls() to register persistent data, such as the disk queue "
                  "name, which might change",
                  evt_tag_str("url", self->load_balancer->targets[0].url));
    }
  /* we need to set up url before we call the inherited init method, so our stats key is correct */
  self->url = self->load_balancer->targets[0].url;

  if (self->auth_header && !http_auth_header_init(self->auth_header))
    return FALSE;

  _load_auth_header(s);

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  http_load_balancer_set_recovery_timeout(self->load_balancer, self->super.time_reopen);

  return log_threaded_dest_driver_start_workers(&self->super);
}

static void
http_dd_free(LogPipe *s)
{
  HTTPDestinationDriver *self = (HTTPDestinationDriver *)s;

  log_template_options_destroy(&self->template_options);

  g_string_free(self->delimiter, TRUE);
  g_string_free(self->body_prefix, TRUE);
  g_string_free(self->body_suffix, TRUE);

  curl_global_cleanup();

  g_free(self->user);
  g_free(self->password);
  g_free(self->user_agent);
  g_free(self->ca_dir);
  g_free(self->ca_file);
  g_free(self->cert_file);
  g_free(self->key_file);
  g_free(self->ciphers);
  g_list_free_full(self->headers, g_free);
  http_auth_header_free(self->auth_header);
  g_mutex_free(self->workers_lock);
  http_load_balancer_free(self->load_balancer);

  log_threaded_dest_driver_free(s);
}

LogDriver *
http_dd_new(GlobalConfig *cfg)
{
  HTTPDestinationDriver *self = g_new0(HTTPDestinationDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = http_dd_init;
  self->super.super.super.super.free_fn = http_dd_free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.format_stats_instance = _format_stats_instance;
  self->super.stats_source = SCS_HTTP;
  self->super.worker.construct = http_dw_new;

  curl_global_init(CURL_GLOBAL_ALL);

  self->ssl_version = CURL_SSLVERSION_DEFAULT;
  self->peer_verify = TRUE;
  /* disable batching even if the global batch_lines is specified */
  self->super.batch_lines = 0;
  self->batch_bytes = 0;
  self->body_prefix = g_string_new("");
  self->body_suffix = g_string_new("");
  self->delimiter = g_string_new("\n");
  self->workers_lock = g_mutex_new();
  self->load_balancer = http_load_balancer_new();
  curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
  if (!self->user_agent)
    self->user_agent = g_strdup_printf("syslog-ng %s/libcurl %s",
                                       SYSLOG_NG_VERSION, curl_info->version);

  return &self->super.super.super;
}
