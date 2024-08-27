/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2018-2022 One Identity LLC.
 * Copyright (c) 2018 Balazs Scheidler
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
#include "http-worker.h"
#include "http.h"

#include "syslog-names.h"
#include "scratch-buffers.h"
#include "http-signals.h"

#define HTTP_HEADER_FORMAT_ERROR http_header_format_error_quark()

static GQuark http_header_format_error_quark(void)
{
  return g_quark_from_static_string("http_header_format_error_quark");
}

enum HttpHeaderFormatError
{
  HTTP_HEADER_FORMAT_SLOT_CRITICAL_ERROR,
  HTTP_HEADER_FORMAT_SLOT_NON_CRITICAL_ERROR
};

/* HTTPDestinationWorker */

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
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) userp;
  if (!G_UNLIKELY(trace_flag))
    return 0;

  g_assert(type < sizeof(curl_infotype_to_text)/sizeof(curl_infotype_to_text[0]));

  const gchar *text = curl_infotype_to_text[type];
  gchar *sanitized = _sanitize_curl_debug_message(data, size);
  msg_trace("cURL debug",
            evt_tag_int("worker", self->super.worker_index),
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

/* Set up options that are static over the course of a single configuration,
 * request specific options will be set separately
 */
static void
_setup_static_options_in_curl(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  curl_easy_reset(self->curl);

  curl_easy_setopt(self->curl, CURLOPT_WRITEFUNCTION, _curl_write_function);

  curl_easy_setopt(self->curl, CURLOPT_URL, owner->url);

  if (owner->user)
    curl_easy_setopt(self->curl, CURLOPT_USERNAME, owner->user);

  if (owner->password)
    curl_easy_setopt(self->curl, CURLOPT_PASSWORD, owner->password);

  if (owner->user_agent)
    curl_easy_setopt(self->curl, CURLOPT_USERAGENT, owner->user_agent);

  if (owner->ca_dir)
    curl_easy_setopt(self->curl, CURLOPT_CAPATH, owner->ca_dir);

  if (owner->ca_file)
    curl_easy_setopt(self->curl, CURLOPT_CAINFO, owner->ca_file);

  if (owner->cert_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLCERT, owner->cert_file);

  if (owner->key_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLKEY, owner->key_file);

  if (owner->ciphers)
    curl_easy_setopt(self->curl, CURLOPT_SSL_CIPHER_LIST, owner->ciphers);

#if SYSLOG_NG_HAVE_DECL_CURLOPT_TLS13_CIPHERS
  if (owner->tls13_ciphers)
    curl_easy_setopt(self->curl, CURLOPT_TLS13_CIPHERS, owner->tls13_ciphers);
#endif

#if SYSLOG_NG_HAVE_DECL_CURLOPT_SSL_VERIFYSTATUS
  if (owner->ocsp_stapling_verify)
    curl_easy_setopt(self->curl, CURLOPT_SSL_VERIFYSTATUS, 1L);
#endif

  if (owner->proxy)
    curl_easy_setopt(self->curl, CURLOPT_PROXY, owner->proxy);

  curl_easy_setopt(self->curl, CURLOPT_SSLVERSION, owner->ssl_version);
  curl_easy_setopt(self->curl, CURLOPT_SSL_VERIFYHOST, owner->peer_verify ? 2L : 0L);
  curl_easy_setopt(self->curl, CURLOPT_SSL_VERIFYPEER, owner->peer_verify ? 1L : 0L);

  curl_easy_setopt(self->curl, CURLOPT_DEBUGFUNCTION, _curl_debug_function);
  curl_easy_setopt(self->curl, CURLOPT_DEBUGDATA, self);
  curl_easy_setopt(self->curl, CURLOPT_VERBOSE, 1L);

  if (owner->accept_redirects)
    {
      curl_easy_setopt(self->curl, CURLOPT_FOLLOWLOCATION, 1);
      curl_easy_setopt(self->curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
#if SYSLOG_NG_HAVE_DECL_CURLOPT_REDIR_PROTOCOLS_STR
      curl_easy_setopt(self->curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
      curl_easy_setopt(self->curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
      curl_easy_setopt(self->curl, CURLOPT_MAXREDIRS, 3);
    }
  curl_easy_setopt(self->curl, CURLOPT_TIMEOUT, owner->timeout);

  if (owner->method_type == METHOD_TYPE_PUT)
    curl_easy_setopt(self->curl, CURLOPT_CUSTOMREQUEST, "PUT");

  curl_easy_setopt(self->curl, CURLOPT_ACCEPT_ENCODING, owner->accept_encoding->str);

  curl_easy_setopt(self->curl, CURLOPT_NOSIGNAL, 1L);
}


static void
_add_header(List *list, const gchar *header, const gchar *value)
{
  GString *buffer = scratch_buffers_alloc();

  g_string_append(buffer, header);
  g_string_append(buffer, ": ");
  g_string_append(buffer, value);

  list_append(list, buffer->str);
}

static void
_set_error_from_slot_result(const gchar *signal,
                            HttpSlotResultType result,
                            GError **error)
{
  switch (result)
    {
    case HTTP_SLOT_SUCCESS:
    case HTTP_SLOT_RESOLVED:
      g_assert(*error == NULL);
      break;
    case HTTP_SLOT_CRITICAL_ERROR:
      g_set_error(error, HTTP_HEADER_FORMAT_ERROR,
                  HTTP_HEADER_FORMAT_SLOT_CRITICAL_ERROR,
                  "Critical error during slot execution, signal:%s",
                  signal_http_header_request);
      break;
    case HTTP_SLOT_PLUGIN_ERROR:
    default:
      g_set_error(error, HTTP_HEADER_FORMAT_ERROR,
                  HTTP_HEADER_FORMAT_SLOT_NON_CRITICAL_ERROR,
                  "Non-critical error during slot execution, signal:%s",
                  signal_http_header_request);
      break;
    }
}

static void
_collect_rest_headers(HTTPDestinationWorker *self, GError **error)
{
  HttpHeaderRequestSignalData signal_data =
  {
    .result = HTTP_SLOT_SUCCESS,
    .request_headers = self->request_headers,
    .request_body = self->request_body
  };

  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  EMIT(owner->super.super.super.super.signal_slot_connector, signal_http_header_request, &signal_data);

  _set_error_from_slot_result(signal_http_header_request, signal_data.result, error);
}

static void
_add_msg_specific_headers(HTTPDestinationWorker *self, LogMessage *msg)
{
  /* NOTE: I have my doubts that these headers make sense at all.  None of
   * the HTTP collectors I know of, extract this information from the
   * headers and it makes batching several messages into the same request a
   * bit more complicated than it needs to be.  I didn't want to break
   * backward compatibility when batching was introduced, however I think
   * this should eventually be removed */

  _add_header(self->request_headers,
              "X-Syslog-Host",
              log_msg_get_value(msg, LM_V_HOST, NULL));
  _add_header(self->request_headers,
              "X-Syslog-Program",
              log_msg_get_value(msg, LM_V_PROGRAM, NULL));
  _add_header(self->request_headers,
              "X-Syslog-Facility",
              syslog_name_lookup_facility_by_value(msg->pri & SYSLOG_FACMASK));
  _add_header(self->request_headers,
              "X-Syslog-Level",
              syslog_name_lookup_severity_by_value(msg->pri & SYSLOG_PRIMASK));
}

static void
_add_common_headers(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  _add_header(self->request_headers, "Expect", "");
  for (GList *l = owner->headers; l; l = l->next)
    list_append(self->request_headers, l->data);
}

static gboolean
_try_format_request_headers(HTTPDestinationWorker *self, GError **error)
{
  _add_common_headers(self);

  _collect_rest_headers(self, error);

  return (*error == NULL);
}

static void
_add_message_to_batch(HTTPDestinationWorker *self, LogMessage *msg)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  if (self->super.batch_size > 1)
    {
      g_string_append_len(self->request_body, owner->delimiter->str, owner->delimiter->len);
    }
  if (owner->body_template)
    {
      LogTemplateEvalOptions options = {&owner->template_options, LTZ_SEND,
                                        self->super.seq_num, NULL, LM_VT_STRING
                                       };
      log_template_append_format(owner->body_template, msg, &options, self->request_body);
    }
  else
    {
      g_string_append(self->request_body, log_msg_get_value(msg, LM_V_MESSAGE, NULL));
    }
}

static gboolean
_find_http_code_in_list(glong http_code, glong list[])
{
  for (gint i = 0; list[i] != -1; i++)
    if (list[i] == http_code)
      return TRUE;
  return FALSE;
}

static LogThreadedResult
_default_1XX(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  msg_error("http: Server returned with a 1XX (continuation) status code, which was not handled by curl. ",
            evt_tag_str("url", url),
            evt_tag_int("status_code", http_code),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super));

  static glong errors[] = {102, 103, -1};
  if (_find_http_code_in_list(http_code, errors))
    return LTR_ERROR;

  return LTR_NOT_CONNECTED;
}

static LogThreadedResult
_default_3XX(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  msg_notice("http: Server returned with a 3XX (redirect) status code. "
             "Either accept-redirect() is set to no, or this status code is unknown.",
             evt_tag_str("url", url),
             evt_tag_int("status_code", http_code),
             evt_tag_str("driver", owner->super.super.super.id),
             log_pipe_location_tag(&owner->super.super.super.super));
  if (http_code == 304)
    return LTR_ERROR;

  return LTR_NOT_CONNECTED;
}

static LogThreadedResult
_default_4XX(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  msg_notice("http: Server returned with a 4XX (client errors) status code, which means we are not "
             "authorized or the URL is not found.",
             evt_tag_str("url", url),
             evt_tag_int("status_code", http_code),
             evt_tag_str("driver", owner->super.super.super.id),
             log_pipe_location_tag(&owner->super.super.super.super));

  static glong errors[] = {428, -1};
  if (_find_http_code_in_list(http_code, errors))
    return LTR_ERROR;

  static glong drops[] = {410, 416, 422, 424, 425, 451, -1};
  if (_find_http_code_in_list(http_code, drops))
    return LTR_DROP;

  return LTR_NOT_CONNECTED;
}

static LogThreadedResult
_default_5XX(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  msg_notice("http: Server returned with a 5XX (server errors) status code, which indicates server failure.",
             evt_tag_str("url", url),
             evt_tag_int("status_code", http_code),
             evt_tag_str("driver", owner->super.super.super.id),
             log_pipe_location_tag(&owner->super.super.super.super));
  if (http_code == 508)
    return LTR_DROP;

  static glong errors[] = {504, -1};
  if (_find_http_code_in_list(http_code, errors))
    return LTR_ERROR;

  return LTR_NOT_CONNECTED;
}

LogThreadedResult
default_map_http_status_to_worker_status(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  LogThreadedResult retval = LTR_ERROR;

  switch (http_code/100)
    {
    case 1:
      return _default_1XX(self, url, http_code);
    case 2:
      /* everything is dandy */
      return  LTR_SUCCESS;
    case 3:
      return _default_3XX(self, url, http_code);
    case 4:
      return _default_4XX(self, url, http_code);
    case 5:
      return _default_5XX(self, url, http_code);
    default:
      msg_error("http: Unknown HTTP response code",
                evt_tag_str("url", url),
                evt_tag_int("status_code", http_code),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      break;
    }

  return retval;
}

static void
_reinit_request_headers(HTTPDestinationWorker *self)
{
  list_remove_all(self->request_headers);
}

static void
_reinit_request_body(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  g_string_truncate(self->request_body, 0);
  if (self->request_body_compressed != NULL)
    g_string_truncate(self->request_body_compressed, 0);

  if (owner->body_prefix->len > 0)
    g_string_append_len(self->request_body, owner->body_prefix->str, owner->body_prefix->len);

}

static void
_finish_request_body(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  if (owner->body_suffix->len > 0)
    g_string_append_len(self->request_body, owner->body_suffix->str, owner->body_suffix->len);
}

static void
_debug_response_info(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  gdouble total_time = 0;
  glong redirect_count = 0;

  curl_easy_getinfo(self->curl, CURLINFO_TOTAL_TIME, &total_time);
  curl_easy_getinfo(self->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);
  msg_debug("http: HTTP response received",
            evt_tag_str("url", url),
            evt_tag_int("status_code", http_code),
            evt_tag_int("body_size", self->request_body->len),
            evt_tag_int("batch_size", self->super.batch_size),
            evt_tag_int("redirected", redirect_count != 0),
            evt_tag_printf("total_time", "%.3f", total_time),
            evt_tag_int("worker_index", self->super.worker_index),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super));
}

static LogThreadedResult
_custom_map_http_result(HTTPDestinationWorker *self, const gchar *url, HttpResponseHandler *response_handler)
{
  HttpResult result = response_handler->action(response_handler->user_data);
  g_assert(result < HTTP_RESULT_MAX);
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  glong http_code = response_handler->status_code;

  switch (result)
    {
    case HTTP_RESULT_SUCCESS:
      msg_debug("http: handled by response_action",
                evt_tag_str("action", "success"),
                evt_tag_str("url", url),
                evt_tag_int("status_code", http_code),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return LTR_SUCCESS;

    case HTTP_RESULT_RETRY:
      msg_notice("http: handled by response_action",
                 evt_tag_str("action", "retry"),
                 evt_tag_str("url", url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      return LTR_ERROR;

    case HTTP_RESULT_DROP:
      msg_notice("http: handled by response_action",
                 evt_tag_str("action", "drop"),
                 evt_tag_str("url", url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      return LTR_DROP;

    case HTTP_RESULT_DISCONNECT:
      msg_notice("http: handled by response_action",
                 evt_tag_str("action", "disconnect"),
                 evt_tag_str("url", url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      return LTR_NOT_CONNECTED;

    default:
      g_assert_not_reached();
    };

  return LTR_MAX;
}

static gboolean
_curl_perform_request(HTTPDestinationWorker *self, const gchar *url)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  msg_trace("http: Sending HTTP request",
            evt_tag_str("url", url));

  curl_easy_setopt(self->curl, CURLOPT_URL, url);
  if (self->compressor)
    {
      if (compressor_compress(self->compressor, self->request_body_compressed, self->request_body) &&
          self->request_body_compressed->len < self->request_body->len)
        {
          curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->request_body_compressed->str);
          curl_easy_setopt(self->curl, CURLOPT_POSTFIELDSIZE, self->request_body_compressed->len);
          _add_header(self->request_headers, "Content-Encoding", compressor_get_encoding_name(self->compressor));
        }
      else
        {
          msg_debug("http: error compressing data payload, sending uncompressed data instead");
          curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->request_body->str);
        }
    }
  else
    curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->request_body->str);
  curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, http_curl_header_list_as_slist(self->request_headers));

  CURLcode ret = curl_easy_perform(self->curl);
  if (ret != CURLE_OK)
    {
      msg_error("http: error sending HTTP request",
                evt_tag_str("url", url),
                evt_tag_str("error", curl_easy_strerror(ret)),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_curl_get_status_code(HTTPDestinationWorker *self, const gchar *url, glong *http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  CURLcode ret = curl_easy_getinfo(self->curl, CURLINFO_RESPONSE_CODE, http_code);

  if (ret != CURLE_OK)
    {
      msg_error("http: error querying response code",
                evt_tag_str("url", url),
                evt_tag_str("error", curl_easy_strerror(ret)),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}


static LogThreadedResult
_try_to_custom_map_http_status_to_worker_status(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HttpResponseHandler *response_handler = NULL;

  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  if ((response_handler = http_response_handlers_lookup(owner->response_handlers, http_code)))
    return _custom_map_http_result(self, url, response_handler);

  return LTR_MAX;
}

static LogThreadedResult
_map_http_status_code(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  LogThreadedResult result = _try_to_custom_map_http_status_to_worker_status(self, url, http_code);

  if (result != LTR_MAX)
    return result;

  return default_map_http_status_to_worker_status(self, url, http_code);
}

static void
_update_status_code_metrics(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  gint level = log_pipe_is_internal(&self->super.owner->super.super.super) ? STATS_LEVEL3 : STATS_LEVEL1;

  dyn_metrics_store_reset_labels(self->metrics.cache);

  StatsClusterLabel *url_label = dyn_metrics_store_alloc_label(self->metrics.cache);
  url_label->name = "url";
  url_label->value = url;

  StatsClusterLabel *response_code_label = dyn_metrics_store_alloc_label(self->metrics.cache);
  g_snprintf(self->metrics.requests_response_code_str_buffer, sizeof(self->metrics.requests_response_code_str_buffer),
             "%ld", http_code);
  response_code_label->name = "response_code";
  response_code_label->value = self->metrics.requests_response_code_str_buffer;

  StatsClusterLabel *driver_label = dyn_metrics_store_alloc_label(self->metrics.cache);
  driver_label->name = "driver";
  driver_label->value = "http";

  StatsClusterLabel *id_label = dyn_metrics_store_alloc_label(self->metrics.cache);
  id_label->name = "id";
  id_label->value = self->super.owner->super.super.id;

  StatsClusterKey key;
  stats_cluster_single_key_set(&key, "output_http_requests_total",
                               dyn_metrics_store_get_labels(self->metrics.cache),
                               dyn_metrics_store_get_labels_len(self->metrics.cache));

  StatsCounterItem *counter = dyn_metrics_store_retrieve_counter(self->metrics.cache, &key, level);
  stats_counter_inc(counter);
}

static LogThreadedResult
_flush_on_target(HTTPDestinationWorker *self, const gchar *url)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  if (!_curl_perform_request(self, url))
    return LTR_NOT_CONNECTED;

  glong http_code = 0;

  if (!_curl_get_status_code(self, url, &http_code))
    return LTR_NOT_CONNECTED;

  if (debug_flag)
    _debug_response_info(self, url, http_code);

  _update_status_code_metrics(self, url, http_code);

  HttpResponseReceivedSignalData signal_data =
  {
    .result = HTTP_SLOT_SUCCESS,
    .http_code = http_code
  };

  EMIT(owner->super.super.super.super.signal_slot_connector, signal_http_response_received, &signal_data);

  if (signal_data.result == HTTP_SLOT_RESOLVED)
    {
      msg_debug("http: HTTP error resolved issue, retry",
                evt_tag_long("http_code", http_code));
      return LTR_RETRY;
    }

  return _map_http_status_code(self, url, http_code);
}

static gboolean
_format_request_headers_error_is_critical(GError *error)
{
  return (error->code == HTTP_HEADER_FORMAT_SLOT_CRITICAL_ERROR);
}

static void
_format_request_headers_report_error(GError *error)
{
  if (error->code == HTTP_HEADER_FORMAT_SLOT_CRITICAL_ERROR)
    {
      msg_error("http: Failed to format HTTP request headers",
                evt_tag_str("reason", error->message),
                evt_tag_printf("action", "request disconnect"));
    }
  else
    {
      msg_warning("http: Failed to format HTTP request headers",
                  evt_tag_str("reason", error->message),
                  evt_tag_printf("action", "trying to send the request"));
    }
}

static gboolean
_format_request_headers_catch_error(GError **error)
{
  g_assert((*error)->domain == HTTP_HEADER_FORMAT_ERROR);

  _format_request_headers_report_error(*error);
  gboolean unhandled = _format_request_headers_error_is_critical(*error);
  g_clear_error(error);

  return !unhandled;
}

static const gchar *
_get_url(HTTPDestinationWorker *self, HTTPLoadBalancerTarget *target)
{
  if (!http_lb_target_is_url_templated(target))
    return http_lb_target_get_literal_url(target);

  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  http_lb_target_format_templated_url(target, self->msg_for_templated_url, &owner->template_options, self->url_buffer);
  return self->url_buffer->str;
}

/* we flush the accumulated data if
 *   1) we reach batch_size,
 *   2) the message queue becomes empty
 */
static LogThreadedResult
_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) s->owner;
  HTTPLoadBalancerTarget *target, *alt_target = NULL;
  LogThreadedResult retval = LTR_NOT_CONNECTED;
  gint retry_attempts = owner->load_balancer->num_targets;
  GError *error = NULL;

  if (self->super.batch_size == 0)
    return LTR_SUCCESS;

  if (mode == LTF_FLUSH_EXPEDITE)
    return LTR_RETRY;

  _finish_request_body(self);

  if (!_try_format_request_headers(self, &error))
    {
      if (!_format_request_headers_catch_error(&error))
        return LTR_NOT_CONNECTED;
    }

  target = http_load_balancer_choose_target(owner->load_balancer, &self->lbc);
  const gchar *url = _get_url(self, target);

  while (--retry_attempts >= 0)
    {
      retval = _flush_on_target(self, url);
      if (retval == LTR_SUCCESS)
        {
          gsize msg_length = self->request_body->len;
          log_threaded_dest_worker_written_bytes_add(&self->super, msg_length);
          log_threaded_dest_driver_insert_batch_length_stats(self->super.owner, msg_length);

          http_load_balancer_set_target_successful(owner->load_balancer, target);
          break;
        }
      http_load_balancer_set_target_failed(owner->load_balancer, target);

      alt_target = http_load_balancer_choose_target(owner->load_balancer, &self->lbc);
      if (alt_target == target)
        {
          msg_debug("http: Target server down, but no alternative server available. Falling back to retrying after time-reopen()",
                    evt_tag_str("url", url),
                    evt_tag_int("worker_index", self->super.worker_index),
                    evt_tag_str("driver", owner->super.super.super.id),
                    log_pipe_location_tag(&owner->super.super.super.super));
          break;
        }

      const gchar *alt_url = _get_url(self, alt_target);
      msg_debug("http: Target server down, trying an alternative server",
                evt_tag_str("url", url),
                evt_tag_str("alternative_url", alt_url),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));

      target = alt_target;
      url = alt_url;
    }

  _reinit_request_headers(self);
  _reinit_request_body(self);

  log_msg_unref(self->msg_for_templated_url);
  self->msg_for_templated_url = NULL;

  return retval;
}

static gboolean
_should_initiate_flush(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  return (owner->batch_bytes && self->request_body->len + owner->body_suffix->len >= owner->batch_bytes);

}

static LogThreadedResult
_insert_batched(LogThreadedDestWorker *s, LogMessage *msg)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  gsize orig_msg_len = self->request_body->len;
  _add_message_to_batch(self, msg);
  gsize diff_msg_len = self->request_body->len - orig_msg_len;
  log_threaded_dest_driver_insert_msg_length_stats(self->super.owner, diff_msg_len);

  if (!self->msg_for_templated_url)
    self->msg_for_templated_url = log_msg_ref(msg);

  if (_should_initiate_flush(self))
    {
      return log_threaded_dest_worker_flush(&self->super, LTF_FLUSH_NORMAL);
    }
  return LTR_QUEUED;
}

static LogThreadedResult
_insert_single(LogThreadedDestWorker *s, LogMessage *msg)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  gsize orig_msg_len = self->request_body->len;
  _add_message_to_batch(self, msg);
  gsize diff_msg_len = self->request_body->len - orig_msg_len;
  log_threaded_dest_driver_insert_msg_length_stats(self->super.owner, diff_msg_len);

  _add_msg_specific_headers(self, msg);

  self->msg_for_templated_url = log_msg_ref(msg);

  return log_threaded_dest_worker_flush(&self->super, LTF_FLUSH_NORMAL);
}

static gboolean
_init(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  if (http_load_balancer_is_url_templated(owner->load_balancer))
    {
      self->url_buffer = g_string_new(NULL);
    }

  self->request_body = g_string_sized_new(32768);
  if (owner->content_compression != CURL_COMPRESSION_UNCOMPRESSED)
    {
      self->request_body_compressed = g_string_sized_new(32768);
      self->compressor = construct_compressor_by_type(owner->content_compression);
    }
  self->request_headers = http_curl_header_list_new();
  if (!(self->curl = curl_easy_init()))
    {
      msg_error("http: cannot initialize libcurl",
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }
  _setup_static_options_in_curl(self);
  _reinit_request_headers(self);
  _reinit_request_body(self);
  return log_threaded_dest_worker_init_method(s);
}

static void
_deinit(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  if (self->url_buffer)
    g_string_free(self->url_buffer, TRUE);

  g_string_free(self->request_body, TRUE);
  if (self->request_body_compressed)
    g_string_free(self->request_body_compressed, TRUE);

  if (self->compressor)
    compressor_free(self->compressor);
  list_free(self->request_headers);
  curl_easy_cleanup(self->curl);
  log_threaded_dest_worker_deinit_method(s);
}

static void
http_dw_free(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  dyn_metrics_store_free(self->metrics.cache);
  http_lb_client_deinit(&self->lbc);
  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
http_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  HTTPDestinationWorker *self = g_new0(HTTPDestinationWorker, 1);
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) o;

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.flush = _flush;
  self->super.free_fn = http_dw_free;

  if (owner->super.batch_lines > 0 || owner->batch_bytes > 0)
    self->super.insert = _insert_batched;
  else
    self->super.insert = _insert_single;

  self->metrics.cache = dyn_metrics_store_new();

  http_lb_client_init(&self->lbc, owner->load_balancer);
  return &self->super;
}
