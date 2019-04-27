/*
 * Copyright (c) 2018 Balazs Scheidler
 * Copyright (c) 2018 One Identity
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

  if (owner->ca_dir || !owner->use_system_cert_store)
    curl_easy_setopt(self->curl, CURLOPT_CAPATH, owner->ca_dir);

  if (owner->ca_file || !owner->use_system_cert_store)
    curl_easy_setopt(self->curl, CURLOPT_CAINFO, owner->ca_file);

  if (owner->cert_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLCERT, owner->cert_file);

  if (owner->key_file)
    curl_easy_setopt(self->curl, CURLOPT_SSLKEY, owner->key_file);

  if (owner->ciphers)
    curl_easy_setopt(self->curl, CURLOPT_SSL_CIPHER_LIST, owner->ciphers);

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
      curl_easy_setopt(self->curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
      curl_easy_setopt(self->curl, CURLOPT_MAXREDIRS, 3);
    }
  curl_easy_setopt(self->curl, CURLOPT_TIMEOUT, owner->timeout);

  if (owner->method_type == METHOD_TYPE_PUT)
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
_append_auth_header(struct curl_slist *list, HTTPDestinationDriver *owner)
{
  const gchar *auth_header_str = http_auth_header_get_as_string(owner->auth_header);

  if (!auth_header_str)
    {
      if (!http_dd_auth_header_renew(&owner->super.super.super))
        {
          msg_warning("WARNING: failed to renew auth header",
                      evt_tag_str("driver", owner->super.super.super.id),
                      log_pipe_location_tag(&owner->super.super.super.super));
          return list;
        }
      auth_header_str = http_auth_header_get_as_string(owner->auth_header);
    }

  if (auth_header_str)
    {
      list = curl_slist_append(list, auth_header_str);
    }
  else
    {
      msg_warning("WARNING: auth-header() returned NULL-value",
                  evt_tag_str("driver", owner->super.super.super.id),
                  log_pipe_location_tag(&owner->super.super.super.super));
    }

  return list;
}

static struct curl_slist *
_format_request_headers(HTTPDestinationWorker *self, LogMessage *msg)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
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

  for (l = owner->headers; l; l = l->next)
    headers = curl_slist_append(headers, l->data);

  if (owner->auth_header)
    headers = _append_auth_header(headers, owner);

  return headers;
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
      log_template_append_format(owner->body_template, msg, &owner->template_options, LTZ_SEND,
                                 self->super.seq_num, NULL, self->request_body);
    }
  else
    {
      g_string_append(self->request_body, log_msg_get_value(msg, LM_V_MESSAGE, NULL));
    }
}

LogThreadedResult
map_http_status_to_worker_status(HTTPDestinationWorker *self, const gchar *url, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  LogThreadedResult retval = LTR_ERROR;

  switch (http_code/100)
    {
    case 1:
      msg_error("Server returned with a 1XX (continuation) status code, which was not handled by curl. "
                "Trying again",
                evt_tag_str("url", owner->url),
                evt_tag_int("status_code", http_code),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      break;
    case 2:
      /* everything is dandy */
      retval = LTR_SUCCESS;
      break;
    case 3:
      msg_notice("Server returned with a 3XX (redirect) status code, which was not handled by curl. "
                 "Either accept-redirect() is set to no, or this status code is unknown. Trying again",
                 evt_tag_str("url", url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      break;
    case 4:
      msg_notice("Server returned with a 4XX (client errors) status code, which means we are not "
                 "authorized or the URL is not found.",
                 evt_tag_str("url", url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      retval = LTR_DROP;
      break;
    case 5:
      msg_notice("Server returned with a 5XX (server errors) status code, which indicates server failure. "
                 "Trying again",
                 evt_tag_str("url", owner->url),
                 evt_tag_int("status_code", http_code),
                 evt_tag_str("driver", owner->super.super.super.id),
                 log_pipe_location_tag(&owner->super.super.super.super));
      break;
    default:
      msg_error("Unknown HTTP response code",
                evt_tag_str("url", url),
                evt_tag_int("status_code", http_code),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      break;
    }

  return retval;
}

static void
_reinit_request_body(HTTPDestinationWorker *self)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  g_string_truncate(self->request_body, 0);
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
_debug_response_info(HTTPDestinationWorker *self, HTTPLoadBalancerTarget *target, glong http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  gdouble total_time = 0;
  glong redirect_count = 0;

  curl_easy_getinfo(self->curl, CURLINFO_TOTAL_TIME, &total_time);
  curl_easy_getinfo(self->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);
  msg_debug("curl: HTTP response received",
            evt_tag_str("url", target->url),
            evt_tag_int("status_code", http_code),
            evt_tag_int("body_size", self->request_body->len),
            evt_tag_int("batch_size", self->super.batch_size),
            evt_tag_int("redirected", redirect_count != 0),
            evt_tag_printf("total_time", "%.3f", total_time),
            evt_tag_int("worker_index", self->super.worker_index),
            evt_tag_str("driver", owner->super.super.super.id),
            log_pipe_location_tag(&owner->super.super.super.super));
}

static gboolean
_curl_perform_request(HTTPDestinationWorker *self, HTTPLoadBalancerTarget *target)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  msg_trace("Sending HTTP request",
            evt_tag_str("url", target->url));

  curl_easy_setopt(self->curl, CURLOPT_URL, target->url);
  curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, self->request_headers);
  curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->request_body->str);

  CURLcode ret = curl_easy_perform(self->curl);
  if (ret != CURLE_OK)
    {
      msg_error("curl: error sending HTTP request",
                evt_tag_str("url", target->url),
                evt_tag_str("error", curl_easy_strerror(ret)),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_curl_get_status_code(HTTPDestinationWorker *self, HTTPLoadBalancerTarget *target, glong *http_code)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;
  CURLcode ret = curl_easy_getinfo(self->curl, CURLINFO_RESPONSE_CODE, http_code);

  if (ret != CURLE_OK)
    {
      msg_error("curl: error querying response code",
                evt_tag_str("url", target->url),
                evt_tag_str("error", curl_easy_strerror(ret)),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static LogThreadedResult
_renew_header(HTTPDestinationDriver *self)
{
  if (!http_dd_auth_header_renew(&self->super.super.super))
    return LTR_NOT_CONNECTED;
  return LTR_RETRY;
}

static LogThreadedResult
_flush_on_target(HTTPDestinationWorker *self, HTTPLoadBalancerTarget *target)
{
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  if (!_curl_perform_request(self, target))
    return LTR_NOT_CONNECTED;

  glong http_code = 0;

  if (!_curl_get_status_code(self, target, &http_code))
    return LTR_NOT_CONNECTED;

  if (debug_flag)
    _debug_response_info(self, target, http_code);

  if (http_code == 401 && owner->auth_header)
    return _renew_header(owner);

  return map_http_status_to_worker_status(self, target->url, http_code);
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

  if (self->super.batch_size == 0)
    return LTR_SUCCESS;

  if (mode == LTF_FLUSH_EXPEDITE)
    return LTR_RETRY;

  _finish_request_body(self);

  target = http_load_balancer_choose_target(owner->load_balancer, &self->lbc);

  while (--retry_attempts >= 0)
    {
      retval = _flush_on_target(self, target);
      if (retval == LTR_SUCCESS)
        {
          http_load_balancer_set_target_successful(owner->load_balancer, target);
          break;
        }
      http_load_balancer_set_target_failed(owner->load_balancer, target);

      alt_target = http_load_balancer_choose_target(owner->load_balancer, &self->lbc);
      if (alt_target == target)
        {
          msg_debug("Target server down, but no alternative server available. Falling back to retrying after time-reopen()",
                    evt_tag_str("url", target->url),
                    evt_tag_int("worker_index", self->super.worker_index),
                    evt_tag_str("driver", owner->super.super.super.id),
                    log_pipe_location_tag(&owner->super.super.super.super));
          break;
        }

      msg_debug("Target server down, trying an alternative server",
                evt_tag_str("url", target->url),
                evt_tag_str("alternative_url", alt_target->url),
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));

      target = alt_target;
    }
  _reinit_request_body(self);
  curl_slist_free_all(self->request_headers);
  self->request_headers = NULL;
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

  if (self->request_headers == NULL)
    self->request_headers = _format_request_headers(self, NULL);

  _add_message_to_batch(self, msg);

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

  self->request_headers = _format_request_headers(self, msg);
  _add_message_to_batch(self, msg);
  return log_threaded_dest_worker_flush(&self->super, LTF_FLUSH_NORMAL);
}

static gboolean
_thread_init(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) self->super.owner;

  self->request_body = g_string_sized_new(32768);
  if (!(self->curl = curl_easy_init()))
    {
      msg_error("curl: cannot initialize libcurl",
                evt_tag_int("worker_index", self->super.worker_index),
                evt_tag_str("driver", owner->super.super.super.id),
                log_pipe_location_tag(&owner->super.super.super.super));
      return FALSE;
    }
  _setup_static_options_in_curl(self);
  _reinit_request_body(self);
  return log_threaded_dest_worker_init_method(s);
}

static void
_thread_deinit(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  g_string_free(self->request_body, TRUE);
  curl_easy_cleanup(self->curl);
  log_threaded_dest_worker_deinit_method(s);
}

static void
http_dw_free(LogThreadedDestWorker *s)
{
  HTTPDestinationWorker *self = (HTTPDestinationWorker *) s;

  http_lb_client_deinit(&self->lbc);
  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
http_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  HTTPDestinationWorker *self = g_new0(HTTPDestinationWorker, 1);
  HTTPDestinationDriver *owner = (HTTPDestinationDriver *) o;

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);
  self->super.thread_init = _thread_init;
  self->super.thread_deinit = _thread_deinit;
  self->super.flush = _flush;
  self->super.free_fn = http_dw_free;

  if (owner->super.batch_lines > 0 || owner->batch_bytes > 0)
    self->super.insert = _insert_batched;
  else
    self->super.insert = _insert_single;

  http_lb_client_init(&self->lbc, owner->load_balancer);
  return &self->super;
}
