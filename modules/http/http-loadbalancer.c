/*
 * Copyright (c) 2018 Balazs Scheidler
 * Copyright (c) 2018 Balabit
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

#include "http-loadbalancer.h"
#include "messages.h"
#include "str-utils.h"
#include <string.h>
#include "compat/curl.h"

#define HTTP_URL_FORMAT_ERROR http_url_format_error_quark()

static GQuark http_url_format_error_quark(void)
{
  return g_quark_from_static_string("http_url_format_error_quark");
}

enum HttpUrlFormatError
{
  HTTP_URL_FORMAT_ERROR_UNSAFE_TEMPLATE,
};

/* HTTPLoadBalancerTarget */

static gboolean
_is_url_safely_templated(const gchar *url, GError **error)
{
#if !SYSLOG_NG_CURL_FULLY_SUPPORTS_URL_PARSING
  if (strchr(url, '$'))
    msg_warning_once("http(): Cannot validate whether the url() option is safely templated or not with the libcurl "
                     "version your syslog-ng was compiled with. Using templates in the scheme, host, port, user "
                     "or password part of the URL is considered unsafe and not recommended. Please make sure that only "
                     "the path or query parameters parts are templated in your http() destinations");
  return TRUE;
#else
  if (!strchr(url, '$'))
    return TRUE;

  static const gchar *unsafe_part_names[CURLUE_LAST] =
  {
    [CURLUE_BAD_SCHEME] = "Scheme",
    [CURLUE_BAD_HOSTNAME] = "Host",
    [CURLUE_BAD_PORT_NUMBER] = "Port",
    [CURLUE_BAD_USER] = "User",
    [CURLUE_BAD_PASSWORD] = "Password",

    [CURLUE_MALFORMED_INPUT] = "Failed to parse URL. Scheme, host, port, user or password",
  };

  static const struct
  {
    CURLUPart part;
    CURLUcode error;
  } unsafe_parts[] =
  {
    { .part = CURLUPART_SCHEME, .error = CURLUE_BAD_SCHEME },
    { .part = CURLUPART_HOST, .error = CURLUE_BAD_HOSTNAME },
    { .part = CURLUPART_PORT, .error = CURLUE_BAD_PORT_NUMBER },
    { .part = CURLUPART_USER, .error = CURLUE_BAD_USER },
    { .part = CURLUPART_PASSWORD, .error = CURLUE_BAD_PASSWORD },
  };

  CURLU *h = curl_url();
  CURLUcode rc = curl_url_set(h, CURLUPART_URL, url, CURLU_ALLOW_SPACE);

  const gchar *unsafe_part_name = unsafe_part_names[rc];
  if (unsafe_part_name)
    goto exit;

  for (size_t i = 0; i < G_N_ELEMENTS(unsafe_parts) && !unsafe_part_name; i++)
    {
      gchar *part = NULL;
      curl_url_get(h, unsafe_parts[i].part, &part, 0);

      if (part && strchr(part, '$'))
        unsafe_part_name = unsafe_part_names[unsafe_parts[i].error];

      curl_free(part);
    }

exit:
  curl_url_cleanup(h);

  if (unsafe_part_name)
    {
      g_set_error(error, HTTP_URL_FORMAT_ERROR, HTTP_URL_FORMAT_ERROR_UNSAFE_TEMPLATE,
                  "%s part of URL cannot contain templates: %s", unsafe_part_name, url);
      return FALSE;
    }

  return TRUE;
#endif
}

gboolean
http_lb_target_init(HTTPLoadBalancerTarget *self, const gchar *url, gint index_, GError **error)
{
  memset(self, 0, sizeof(*self));

  if (!_is_url_safely_templated(url, error))
    return FALSE;

  LogTemplate *url_template = log_template_new(configuration, NULL);
  log_template_set_escape(url_template, TRUE);
  if (!log_template_compile(url_template, url, error))
    {
      log_template_unref(url_template);
      return FALSE;
    }

  log_template_unref(self->url_template);
  self->url_template = url_template;
  self->state = HTTP_TARGET_OPERATIONAL;
  self->index = index_;
  g_snprintf(self->formatted_index, sizeof(self->formatted_index), "%d", index_);

  return TRUE;
}

void
http_lb_target_deinit(HTTPLoadBalancerTarget *self)
{
  log_template_unref(self->url_template);
}

gboolean
http_lb_target_is_url_templated(HTTPLoadBalancerTarget *self)
{
  return !log_template_is_literal_string(self->url_template);
}

const gchar *
http_lb_target_get_literal_url(HTTPLoadBalancerTarget *self)
{
  return log_template_get_literal_value(self->url_template, NULL);
}

static void
_escape_urlencoded(GString *result, const gchar *value, gsize value_len)
{
  APPEND_ZERO(value, value, value_len);
  g_string_append_uri_escaped(result, value, NULL, FALSE);
}

void
http_lb_target_format_templated_url(HTTPLoadBalancerTarget *self, LogMessage *msg,
                                    const LogTemplateOptions *template_options, GString *result)
{
  LogTemplateEvalOptions template_eval_options =
  {
    .opts = template_options,
    .tz = LTZ_LOCAL,
    .seq_num = -1,
    .context_id = self->formatted_index,
    .context_id_type = LM_VT_INTEGER,
    .escape = _escape_urlencoded,
  };
  log_template_format(self->url_template, msg, &template_eval_options, result);
}

/* HTTPLoadBalancerClient */

void
http_lb_client_init(HTTPLoadBalancerClient *self, HTTPLoadBalancer *lb)
{
  memset(self, 0, sizeof(*self));
  http_load_balancer_track_client(lb, self);
}

void
http_lb_client_deinit(HTTPLoadBalancerClient *self)
{
}

/* HTTPLoadBalancer */

static void
_recalculate_clients_per_target_goals(HTTPLoadBalancer *self)
{
  gint num_operational_targets = self->num_targets - self->num_failed_targets;

  if (num_operational_targets == 0)
    return;

  /* spread clients evenly */
  gint clients_per_target = self->num_clients / num_operational_targets;
  gint remainder = self->num_clients % num_operational_targets;

  /* assign the new targets */
  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[i];

      if (target->state != HTTP_TARGET_OPERATIONAL)
        continue;

      target->max_clients = clients_per_target;
      if (remainder > 0)
        {
          target->max_clients++;
          remainder--;
        }
      msg_trace("Setting maximum number of workers for HTTP destination",
                evt_tag_str("url", target->url_template->template_str),
                evt_tag_int("max_clients", target->max_clients));
    }
}

static void
_switch_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget *new_target)
{
  /* self->lock must be held */
  if (lbc->target != new_target)
    {
      if (lbc->target)
        lbc->target->number_of_clients--;

      new_target->number_of_clients++;
      lbc->target = new_target;
    }
}

static HTTPLoadBalancerTarget *
_get_least_recently_tried_failing_target(HTTPLoadBalancer *self)
{
  time_t lru = 0;
  gint lru_index = -1;

  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[i];

      if (target->state != HTTP_TARGET_FAILED)
        continue;

      if (lru_index < 0 || lru > target->last_failure_time)
        {
          lru = target->last_failure_time;
          lru_index = i;
        }
    }
  return &self->targets[lru_index >= 0 ? lru_index : 0];
}

static HTTPLoadBalancerTarget *
_recover_a_failed_target(HTTPLoadBalancer *self)
{
  self->last_recovery_attempt = time(NULL);
  /* allocation failed, try the target that failed the least recently. */
  return _get_least_recently_tried_failing_target(self);
}

static gboolean
_check_recovery(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget **new_target)
{
  if (self->num_failed_targets > 0)
    {
      time_t now = time(NULL);

      if (self->last_recovery_attempt == 0)
        self->last_recovery_attempt = now;

      if (now - self->last_recovery_attempt >= self->recovery_timeout)
        {
          *new_target = _recover_a_failed_target(self);
          return TRUE;
        }
    }
  return FALSE;
}

static HTTPLoadBalancerTarget *
_locate_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  gint start_index = lbc->target
                     ? (lbc->target->index + 1) % self->num_targets
                     : 0;
  for (gint i = 0; i < self->num_targets; i++)
    {
      HTTPLoadBalancerTarget *target = &self->targets[(i + start_index) % self->num_targets];

      if (target->state == HTTP_TARGET_OPERATIONAL &&
          target->number_of_clients < target->max_clients)
        return target;
    }

  return _recover_a_failed_target(self);
}

static gboolean
_check_rebalance(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc, HTTPLoadBalancerTarget **new_target)
{
  /* Are we misbalanced? */
  if (lbc->target == NULL ||
      lbc->target->state != HTTP_TARGET_OPERATIONAL ||
      lbc->target->number_of_clients > lbc->target->max_clients)
    {
      *new_target = _locate_target(self, lbc);
      return TRUE;
    }
  return FALSE;
}

HTTPLoadBalancerTarget *
http_load_balancer_choose_target(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  HTTPLoadBalancerTarget *new_target;

  g_mutex_lock(&self->lock);

  new_target = lbc->target;
  if (_check_recovery(self, lbc, &new_target) ||
      _check_rebalance(self, lbc, &new_target))
    _switch_target(self, lbc, new_target);

  g_mutex_unlock(&self->lock);
  return lbc->target;
}

gboolean
http_load_balancer_add_target(HTTPLoadBalancer *self, const gchar *url, GError **error)
{
  gint n = self->num_targets++;

  self->targets = g_renew(HTTPLoadBalancerTarget, self->targets, self->num_targets);
  return http_lb_target_init(&self->targets[n], url, n, error);
}

void
http_load_balancer_drop_all_targets(HTTPLoadBalancer *self)
{
  for (int i = 0; i < self->num_targets; i++)
    http_lb_target_deinit(&self->targets[i]);
  self->num_targets = 0;
}

void
http_load_balancer_track_client(HTTPLoadBalancer *self, HTTPLoadBalancerClient *lbc)
{
  g_mutex_lock(&self->lock);
  self->num_clients++;
  _recalculate_clients_per_target_goals(self);
  g_mutex_unlock(&self->lock);
}

void
http_load_balancer_set_target_failed(HTTPLoadBalancer *self, HTTPLoadBalancerTarget *target)
{
  g_mutex_lock(&self->lock);
  if (target->state != HTTP_TARGET_FAILED)
    {
      msg_debug("Load balancer target failed, removing from rotation",
                evt_tag_str("url", target->url_template->template_str));
      self->num_failed_targets++;
      target->state = HTTP_TARGET_FAILED;
      _recalculate_clients_per_target_goals(self);
    }
  target->last_failure_time = time(NULL);
  g_mutex_unlock(&self->lock);
}

void
http_load_balancer_set_target_successful(HTTPLoadBalancer *self, HTTPLoadBalancerTarget *target)
{
  g_mutex_lock(&self->lock);
  if (target->state != HTTP_TARGET_OPERATIONAL)
    {
      msg_debug("Load balancer target recovered, adding back to rotation",
                evt_tag_str("url", target->url_template->template_str));
      self->num_failed_targets--;
      target->state = HTTP_TARGET_OPERATIONAL;
      _recalculate_clients_per_target_goals(self);
    }
  g_mutex_unlock(&self->lock);
}

gboolean
http_load_balancer_is_url_templated(HTTPLoadBalancer *self)
{
  for (gint i = 0; i < self->num_targets; i++)
    {
      if (http_lb_target_is_url_templated(&self->targets[i]))
        return TRUE;
    }

  return FALSE;
}

void
http_load_balancer_set_recovery_timeout(HTTPLoadBalancer *self, gint recovery_timeout)
{
  self->recovery_timeout = recovery_timeout;
}

HTTPLoadBalancer *
http_load_balancer_new(void)
{
  HTTPLoadBalancer *self = g_new0(HTTPLoadBalancer, 1);

  g_mutex_init(&self->lock);
  self->recovery_timeout = 60;
  return self;
}

void
http_load_balancer_free(HTTPLoadBalancer *self)
{
  http_load_balancer_drop_all_targets(self);
  g_free(self->targets);
  g_mutex_clear(&self->lock);
  g_free(self);
}
