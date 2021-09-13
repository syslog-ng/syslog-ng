#include "filter-throttle.h"
#include "timeutils/misc.h"
#include "scratch-buffers.h"
#include "str-utils.h"

typedef struct _FilterThrottle
{
  FilterExprNode super;
  NVHandle key_handle;
  gint rate;
  GHashTable *rate_limits;
} FilterThrottle;

typedef struct _ThrottleRateLimit
{
  gchar *key;
  gint tokens;
  GTimeVal last_check;
} ThrottleRateLimit;

ThrottleRateLimit *
throttle_ratelimit_new(const gchar *key)
{
  ThrottleRateLimit *self = g_new0(ThrottleRateLimit, 1);
  if (key)
    self->key = g_strdup(key);
  return self;
}

void
throttle_ratelimit_free(ThrottleRateLimit *self)
{
  if (self->key)
    g_free(self->key);
  g_free(self);
}

static gboolean
filter_throttle_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterThrottle *self = (FilterThrottle *)s;
  if (self->rate == 0)
    return FALSE;

  ThrottleRateLimit *rl;
  const gchar *key;
  gssize len = 0;

  LogMessage *msg = msgs[num_msg-1];
  key = log_msg_get_value(msg, self->key_handle, &len);
  APPEND_ZERO(key, key, len);

  rl = g_hash_table_lookup(self->rate_limits, key);

  GTimeVal now;
  glong diff;
  gint num_new_tokens;
  g_get_current_time(&now);

  if (!rl)
    {
      rl = throttle_ratelimit_new(key);
      g_hash_table_insert(self->rate_limits, &rl->key, rl);
    }

  if (rl->last_check.tv_sec == 0)
    {
      rl->last_check = now;
      rl->tokens = self->rate;
      diff = 0;
    }
  else
    {
      diff = g_time_val_diff(&now, &rl->last_check);
    }

  num_new_tokens = (diff * self->rate) / G_USEC_PER_SEC;
  if (num_new_tokens)
    {
      rl->tokens = MIN(self->rate, rl->tokens + num_new_tokens);
      rl->last_check = now;
    }

  if (num_msg > 0 && rl->tokens > 0)
    {
      rl->tokens -= num_msg;
      return TRUE;
    }

  return FALSE;
}

static void
filter_throttle_free(FilterExprNode *s)
{
  FilterThrottle *self = (FilterThrottle *) s;

  g_hash_table_destroy(self->rate_limits);
}

gboolean
filter_throttle_init(FilterExprNode *s, NVHandle key_handle, gint rate)
{
  FilterThrottle *self = (FilterThrottle *)s;
  self->key_handle = key_handle;
  self->rate = rate;

  return TRUE;
}

FilterExprNode *
filter_throttle_new(void)
{
  FilterThrottle *self = g_new0(FilterThrottle, 1);
  filter_expr_node_init_instance(&self->super);

  self->super.eval = filter_throttle_eval;
  self->super.free_fn = filter_throttle_free;
  self->rate_limits = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)throttle_ratelimit_free);

  return &self->super;
}
