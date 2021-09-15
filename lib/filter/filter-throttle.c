/*
 * Copyright (c) 2021 Balabit
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

#include "filter-throttle.h"
#include "timeutils/misc.h"
#include "scratch-buffers.h"
#include "str-utils.h"

typedef struct _FilterThrottle
{
  FilterExprNode super;
  NVHandle key_handle;
  gint rate;
  GMutex *map_lock;
  GHashTable *rate_limits;
} FilterThrottle;

typedef struct _ThrottleRateLimit
{
  gint tokens;
  GTimeVal last_check;
  GMutex *ratelimit_lock;
} ThrottleRateLimit;

static ThrottleRateLimit *
throttle_ratelimit_new()
{
  ThrottleRateLimit *self = g_new0(ThrottleRateLimit, 1);
  self->ratelimit_lock = g_mutex_new();
  return self;
}

static void
throttle_ratelimit_free(ThrottleRateLimit *self)
{
  g_mutex_free(self->ratelimit_lock);
  g_free(self);
}

static void
throttle_ratelimit_add_new_tokens(ThrottleRateLimit *self, gint rate)
{
  glong diff;
  gint num_new_tokens;
  GTimeVal now;
  g_get_current_time(&now);

  if (self->last_check.tv_sec == 0)
    {
      self->last_check = now;
      self->tokens = rate;
      diff = 0;
    }
  else
    {
      diff = g_time_val_diff(&now, &self->last_check);
    }

  num_new_tokens = (diff * rate) / G_USEC_PER_SEC;
  if (num_new_tokens)
    {
      self->tokens = MIN(rate, self->tokens + num_new_tokens);
      self->last_check = now;
    }
}

static gboolean
throttle_ratelimit_try_consume_tokens(ThrottleRateLimit *self, gint num_tokens)
{
  if (self->tokens >= num_tokens)
    {
      self->tokens -= num_tokens;
      return TRUE;
    }
  return FALSE;
}

static gboolean
filter_throttle_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterThrottle *self = (FilterThrottle *)s;
  if (self->rate == 0)
    return FALSE;

  const gchar *key;
  gssize len = 0;

  if (self->key_handle)
    {
      LogMessage *msg = msgs[num_msg-1];
      key = log_msg_get_value(msg, self->key_handle, &len);
      APPEND_ZERO(key, key, len);
    }
  else
    {
      key = "";
    }

  ThrottleRateLimit *rl;
  gboolean within_ratelimit;

  g_mutex_lock(self->map_lock);
  {
    rl = g_hash_table_lookup(self->rate_limits, key);

    if (!rl)
      {
        rl = throttle_ratelimit_new();
        g_hash_table_insert(self->rate_limits, g_strdup(key), rl);
      }
  }
  g_mutex_unlock(self->map_lock);

  g_mutex_lock(rl->ratelimit_lock);
  {
    throttle_ratelimit_add_new_tokens(rl, self->rate);
    within_ratelimit = throttle_ratelimit_try_consume_tokens(rl, num_msg);
  }
  g_mutex_unlock(rl->ratelimit_lock);

  return within_ratelimit;
}

static void
filter_throttle_free(FilterExprNode *s)
{
  FilterThrottle *self = (FilterThrottle *) s;

  g_hash_table_destroy(self->rate_limits);
  g_mutex_free(self->map_lock);
}

gboolean
filter_throttle_init(FilterExprNode *s, GlobalConfig *cfg)
{
  FilterThrottle *self = (FilterThrottle *)s;

  if (self->rate <= 0)
    {
      msg_error("throttle: the rate() argument is required, and must be non negative in throttle filters");
      return FALSE;
    }

  return TRUE;
}

void filter_throttle_set_key(FilterExprNode *s, NVHandle key_handle)
{
  FilterThrottle *self = (FilterThrottle *)s;
  self->key_handle = key_handle;
}

void filter_throttle_set_rate(FilterExprNode *s, gint rate)
{
  FilterThrottle *self = (FilterThrottle *)s;
  self->rate = rate;
}

FilterExprNode *
filter_throttle_new(void)
{
  FilterThrottle *self = g_new0(FilterThrottle, 1);
  filter_expr_node_init_instance(&self->super);

  self->super.init = filter_throttle_init;
  self->super.eval = filter_throttle_eval;
  self->super.free_fn = filter_throttle_free;
  self->map_lock = g_mutex_new();
  self->rate_limits = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)throttle_ratelimit_free);

  return &self->super;
}
