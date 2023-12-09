/*
 * Copyright (c) 2021 One Identity
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

#include "rate-limit.h"
#include "timeutils/misc.h"
#include "scratch-buffers.h"
#include "str-utils.h"
#include <iv.h>

typedef struct _RateLimit
{
  FilterExprNode super;
  LogTemplate *key_template;
  gint rate;
  GMutex map_lock;
  GHashTable *rate_limits;
} RateLimit;

typedef struct _RateLimiter
{
  gint tokens;
  gint rate;
  struct timespec last_check;
  GMutex lock;
} RateLimiter;

static RateLimiter *
rate_limiter_new(gint rate)
{
  RateLimiter *self = g_new0(RateLimiter, 1);

  iv_validate_now();
  self->last_check = iv_now;
  g_mutex_init(&self->lock);
  self->rate = rate;
  self->tokens = rate;

  return self;
}

static void
rate_limiter_free(RateLimiter *self)
{
  g_mutex_clear(&self->lock);
  g_free(self);
}

static void
rate_limiter_add_new_tokens(RateLimiter *self)
{
  glong usec_since_last_fill;
  gint num_new_tokens;
  struct timespec now;

  iv_validate_now();
  now = iv_now;
  g_mutex_lock(&self->lock);
  {
    usec_since_last_fill = timespec_diff_usec(&now, &self->last_check);

    num_new_tokens = (usec_since_last_fill * self->rate) / G_USEC_PER_SEC;
    if (num_new_tokens)
      {
        self->tokens = MIN(self->rate, self->tokens + num_new_tokens);
        self->last_check = now;
      }
  }
  g_mutex_unlock(&self->lock);
}

static gboolean
rate_limiter_try_consume_tokens(RateLimiter *self, gint num_tokens)
{
  gboolean within_ratelimit;
  g_mutex_lock(&self->lock);
  {
    if (self->tokens >= num_tokens)
      {
        self->tokens -= num_tokens;
        within_ratelimit = TRUE;
      }
    else
      {
        within_ratelimit = FALSE;
      }
  }
  g_mutex_unlock(&self->lock);
  return within_ratelimit;
}

static gboolean
rate_limiter_process_new_logs(RateLimiter *self, gint num_new_logs)
{
  rate_limiter_add_new_tokens(self);
  return rate_limiter_try_consume_tokens(self, num_new_logs);
}

static const gchar *
rate_limit_generate_key(FilterExprNode *s, LogMessage *msg, LogTemplateEvalOptions *options, gssize *len)
{
  RateLimit *self = (RateLimit *)s;

  if(!self->key_template)
    {
      return "";
    }

  if (log_template_is_trivial(self->key_template))
    {
      return log_template_get_trivial_value(self->key_template, msg, len);
    }

  GString *key = scratch_buffers_alloc();

  log_template_format(self->key_template, msg, options, key);

  *len = key->len;

  return key->str;
}

static gboolean
rate_limit_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  RateLimit *self = (RateLimit *)s;

  LogMessage *msg = msgs[num_msg - 1];
  gssize len = 0;
  const gchar *key = rate_limit_generate_key(s, msg, options, &len);
  APPEND_ZERO(key, key, len);

  RateLimiter *rl;

  g_mutex_lock(&self->map_lock);
  {
    rl = g_hash_table_lookup(self->rate_limits, key);

    if (!rl)
      {
        rl = rate_limiter_new(self->rate);
        g_hash_table_insert(self->rate_limits, g_strdup(key), rl);
      }
  }
  g_mutex_unlock(&self->map_lock);

  return rate_limiter_process_new_logs(rl, num_msg) ^ s->comp;
}

static void
rate_limit_free(FilterExprNode *s)
{
  RateLimit *self = (RateLimit *) s;

  log_template_unref(self->key_template);
  g_hash_table_destroy(self->rate_limits);
  g_mutex_clear(&self->map_lock);
}

static gboolean
rate_limit_init(FilterExprNode *s, GlobalConfig *cfg)
{
  RateLimit *self = (RateLimit *)s;

  if (self->rate <= 0)
    {
      msg_error("rate-limit: the rate() argument is required, and must be non zero in rate-limit filters");
      return FALSE;
    }

  return TRUE;
}

void
rate_limit_set_key_template(FilterExprNode *s, LogTemplate *template)
{
  RateLimit *self = (RateLimit *)s;
  log_template_unref(self->key_template);
  self->key_template = log_template_ref(template);
}

void
rate_limit_set_rate(FilterExprNode *s, gint rate)
{
  RateLimit *self = (RateLimit *)s;
  self->rate = rate;
}

static FilterExprNode *
rate_limit_clone(FilterExprNode *s)
{
  RateLimit *self = (RateLimit *)s;

  FilterExprNode *cloned_self = rate_limit_new();
  rate_limit_set_key_template(cloned_self, self->key_template);
  rate_limit_set_rate(cloned_self, self->rate);

  return cloned_self;
}

FilterExprNode *
rate_limit_new(void)
{
  RateLimit *self = g_new0(RateLimit, 1);
  filter_expr_node_init_instance(&self->super);

  self->super.init = rate_limit_init;
  self->super.eval = rate_limit_eval;
  self->super.free_fn = rate_limit_free;
  self->super.clone = rate_limit_clone;
  g_mutex_init(&self->map_lock);
  self->rate_limits = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)rate_limiter_free);

  return &self->super;
}
