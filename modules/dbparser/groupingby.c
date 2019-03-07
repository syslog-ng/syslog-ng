/*
 * Copyright (c) 2015 BalaBit
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

#include "groupingby.h"
#include "correllation.h"
#include "correllation-context.h"
#include "synthetic-message.h"
#include "messages.h"
#include "str-utils.h"
#include "filter/filter-expr.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"
#include <iv.h>

typedef struct _GroupingBy
{
  StatefulParser super;
  GStaticMutex lock;
  struct iv_timer tick;
  TimerWheel *timer_wheel;
  GTimeVal last_tick;
  CorrellationState *correllation;
  LogTemplate *key_template;
  gint timeout;
  CorrellationScope scope;
  SyntheticMessage *synthetic_message;
  FilterExprNode *trigger_condition_expr;
  FilterExprNode *where_condition_expr;
  FilterExprNode *having_condition_expr;
} GroupingBy;

static NVHandle context_id_handle = 0;

void
grouping_by_set_key_template(LogParser *s, LogTemplate *key_template)
{
  GroupingBy *self = (GroupingBy *) s;

  log_template_unref(self->key_template);
  self->key_template = log_template_ref(key_template);
}

void
grouping_by_set_scope(LogParser *s, CorrellationScope scope)
{
  GroupingBy *self = (GroupingBy *) s;

  self->scope = scope;
}

void
grouping_by_set_timeout(LogParser *s, gint timeout)
{
  GroupingBy *self = (GroupingBy *) s;

  self->timeout = timeout;
}

void
grouping_by_set_trigger_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->trigger_condition_expr = filter_expr;
}

void
grouping_by_set_where_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->where_condition_expr = filter_expr;
}

void
grouping_by_set_having_condition(LogParser *s, FilterExprNode *filter_expr)
{
  GroupingBy *self = (GroupingBy *) s;

  self->having_condition_expr = filter_expr;
}

void
grouping_by_set_synthetic_message(LogParser *s, SyntheticMessage *message)
{
  GroupingBy *self = (GroupingBy *) s;

  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  self->synthetic_message = message;
}

/* NOTE: lock should be acquired for writing before calling this function. */
void
grouping_by_set_time(GroupingBy *self, const UnixTime *ls)
{
  GTimeVal now;
  gchar buf[256];

  /* clamp the current time between the timestamp of the current message
   * (low limit) and the current system time (high limit).  This ensures
   * that incorrect clocks do not skew the current time know by the
   * correllation engine too much. */

  cached_g_current_time(&now);
  self->last_tick = now;

  if (ls->ut_sec < now.tv_sec)
    now.tv_sec = ls->ut_sec;

  timer_wheel_set_time(self->timer_wheel, now.tv_sec);
  msg_debug("Advancing grouping-by() current time because of an incoming message",
            evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
            evt_tag_str("location",
                        log_expr_node_format_location(self->super.super.super.expr_node,
                                                      buf, sizeof(buf))));

}

/*
 * This function can be called any time when pattern-db is not processing
 * messages, but we expect the correllation timer to move forward.  It
 * doesn't need to be called absolutely regularly as it'll use the current
 * system time to determine how much time has passed since the last
 * invocation.  See the timing comment at pattern_db_process() for more
 * information.
 */
void
_grouping_by_timer_tick(GroupingBy *self)
{
  GTimeVal now;
  glong diff;
  gchar buf[256];

  g_static_mutex_lock(&self->lock);
  cached_g_current_time(&now);
  diff = g_time_val_diff(&now, &self->last_tick);

  if (diff > 1e6)
    {
      glong diff_sec = (glong)(diff / 1e6);

      timer_wheel_set_time(self->timer_wheel, timer_wheel_get_time(self->timer_wheel) + diff_sec);
      msg_debug("Advancing grouping-by() current time because of timer tick",
                evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
                evt_tag_str("location",
                            log_expr_node_format_location(self->super.super.super.expr_node,
                                                          buf, sizeof(buf))));
      /* update last_tick, take the fraction of the seconds not calculated into this update into account */

      self->last_tick = now;
      g_time_val_add(&self->last_tick, - (glong)(diff - diff_sec * 1e6));
    }
  else if (diff < 0)
    {
      /* time moving backwards, this can only happen if the computer's time
       * is changed.  We don't update patterndb's idea of the time now, wait
       * another tick instead to update that instead.
       */
      self->last_tick = now;
    }
  g_static_mutex_unlock(&self->lock);
}

static void
grouping_by_timer_tick(gpointer s)
{
  GroupingBy *self = (GroupingBy *) s;

  _grouping_by_timer_tick(self);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}

static gboolean
_evaluate_filter(FilterExprNode *expr, CorrellationContext *context)
{
  return filter_expr_eval_with_context(expr,
                                       (LogMessage **) context->messages->pdata,
                                       context->messages->len);
}

static gboolean
_evaluate_having(GroupingBy *self, CorrellationContext *context)
{
  if (!self->having_condition_expr)
    return TRUE;

  return _evaluate_filter(self->having_condition_expr, context);
}

static gboolean
_evaluate_trigger(GroupingBy *self, CorrellationContext *context)
{
  if (!self->trigger_condition_expr)
    return FALSE;

  return _evaluate_filter(self->trigger_condition_expr, context);
}

static void
grouping_by_emit_synthetic(GroupingBy *self, CorrellationContext *context)
{
  LogMessage *msg;

  if (_evaluate_having(self, context))
    {
      GString *buffer = g_string_sized_new(256);

      msg = synthetic_message_generate_with_context(self->synthetic_message, context, buffer);
      stateful_parser_emit_synthetic(&self->super, msg);
      log_msg_unref(msg);
      g_string_free(buffer, TRUE);
    }
  else
    {
      gchar buf[256];

      msg_debug("groupingby() dropping context, because having() is FALSE",
                evt_tag_str("key", context->key.session_id),
                evt_tag_str("location",
                            log_expr_node_format_location(self->super.super.super.expr_node,
                                                          buf, sizeof(buf))));
    }
}

static void
grouping_by_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data)
{
  CorrellationContext *context = user_data;
  GroupingBy *self = (GroupingBy *) timer_wheel_get_associated_data(wheel);
  gchar buf[256];

  msg_debug("Expiring grouping-by() correllation context",
            evt_tag_long("utc", timer_wheel_get_time(wheel)),
            evt_tag_str("context-id", context->key.session_id),
            evt_tag_str("location",
                        log_expr_node_format_location(self->super.super.super.expr_node,
                                                      buf, sizeof(buf))));
  grouping_by_emit_synthetic(self, context);
  g_hash_table_remove(self->correllation->state, &context->key);

  /* correllation_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */
}


static gchar *
grouping_by_format_persist_name(GroupingBy *self)
{
  static gchar persist_name[512];

  g_snprintf(persist_name, sizeof(persist_name), "grouping-by()");
  return persist_name;
}

static gboolean
_perform_groupby(GroupingBy *self, LogMessage *msg)
{
  GString *buffer = g_string_sized_new(32);
  CorrellationContext *context = NULL;
  gchar buf[256];

  g_static_mutex_lock(&self->lock);
  grouping_by_set_time(self, &msg->timestamps[LM_TS_STAMP]);
  if (self->key_template)
    {
      CorrellationKey key;

      log_template_format(self->key_template, msg, NULL, LTZ_LOCAL, 0, NULL, buffer);
      log_msg_set_value(msg, context_id_handle, buffer->str, -1);

      correllation_key_setup(&key, self->scope, msg, buffer->str);
      context = g_hash_table_lookup(self->correllation->state, &key);
      if (!context)
        {
          msg_debug("Correllation context lookup failure, starting a new context",
                    evt_tag_str("key", buffer->str),
                    evt_tag_int("timeout", self->timeout),
                    evt_tag_int("expiration", timer_wheel_get_time(self->timer_wheel) + self->timeout),
                    evt_tag_str("location",
                                log_expr_node_format_location(self->super.super.super.expr_node,
                                                              buf, sizeof(buf))));
          context = correllation_context_new(&key);
          g_hash_table_insert(self->correllation->state, &context->key, context);
          g_string_steal(buffer);
        }
      else
        {
          msg_debug("Correllation context lookup successful",
                    evt_tag_str("key", buffer->str),
                    evt_tag_int("timeout", self->timeout),
                    evt_tag_int("expiration", timer_wheel_get_time(self->timer_wheel) + self->timeout),
                    evt_tag_int("num_messages", context->messages->len),
                    evt_tag_str("location",
                                log_expr_node_format_location(self->super.super.super.expr_node,
                                                              buf, sizeof(buf))));
        }

      g_ptr_array_add(context->messages, log_msg_ref(msg));

      if (_evaluate_trigger(self, context))
        {
          msg_verbose("Correllation trigger() met, closing state",
                      evt_tag_str("key", context->key.session_id),
                      evt_tag_int("timeout", self->timeout),
                      evt_tag_int("num_messages", context->messages->len),
                      evt_tag_str("location",
                                  log_expr_node_format_location(self->super.super.super.expr_node,
                                                                buf, sizeof(buf))));
          /* close down state */
          if (context->timer)
            timer_wheel_del_timer(self->timer_wheel, context->timer);
          grouping_by_expire_entry(self->timer_wheel, timer_wheel_get_time(self->timer_wheel), context);
        }
      else
        {

          if (context->timer)
            {
              timer_wheel_mod_timer(self->timer_wheel, context->timer, self->timeout);
            }
          else
            {
              context->timer = timer_wheel_add_timer(self->timer_wheel, self->timeout, grouping_by_expire_entry,
                                                     correllation_context_ref(context), (GDestroyNotify) correllation_context_unref);
            }
        }
    }
  else
    {
      context = NULL;
    }

  g_static_mutex_unlock(&self->lock);

  if (context)
    log_msg_write_protect(msg);

  g_string_free(buffer, TRUE);
  return TRUE;
}

static gboolean
_evaluate_where(GroupingBy *self, LogMessage **pmsg, const LogPathOptions *path_options)
{
  if (!self->where_condition_expr)
    return TRUE;

  return filter_expr_eval_root(self->where_condition_expr, pmsg, path_options);
}

static gboolean
grouping_by_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const char *input,
                    gsize input_len)
{
  GroupingBy *self = (GroupingBy *) s;

  if (_evaluate_where(self, pmsg, path_options))
    return _perform_groupby(self, log_msg_make_writable(pmsg, path_options));
  return TRUE;
}

static gboolean
grouping_by_init(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!self->synthetic_message)
    {
      msg_error("The aggregate() option for grouping-by() is mandatory",
                log_pipe_location_tag(s));
      return FALSE;
    }
  if (self->timeout < 1)
    {
      msg_error("timeout() needs to be specified explicitly and must be greater than 0 in the grouping-by() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }
  if (!self->key_template)
    {
      msg_error("The key() option is mandatory for the grouping-by() parser",
                log_pipe_location_tag(s));
      return FALSE;
    }

  self->correllation = cfg_persist_config_fetch(cfg, grouping_by_format_persist_name(self));
  if (!self->correllation)
    {
      self->correllation = correllation_state_new();
    }
  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = grouping_by_timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);
  return stateful_parser_init_method(s);
}

static gboolean
grouping_by_deinit(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  cfg_persist_config_add(cfg, grouping_by_format_persist_name(self), self->correllation,
                         (GDestroyNotify) correllation_state_free, FALSE);
  self->correllation = NULL;
  return stateful_parser_deinit_method(s);
}

static LogPipe *
grouping_by_clone(LogPipe *s)
{
  LogParser *cloned;
  GroupingBy *self = (GroupingBy *) s;

  /* FIXME: share state between clones! */
  cloned = grouping_by_new(s->cfg);
  grouping_by_set_key_template(cloned, self->key_template);
  grouping_by_set_timeout(cloned, self->timeout);
  return &cloned->super;
}

static void
grouping_by_free(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;

  g_static_mutex_free(&self->lock);
  log_template_unref(self->key_template);
  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  timer_wheel_free(self->timer_wheel);
  stateful_parser_free_method(s);
}

LogParser *
grouping_by_new(GlobalConfig *cfg)
{
  GroupingBy *self = g_new0(GroupingBy, 1);

  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = grouping_by_free;
  self->super.super.super.init = grouping_by_init;
  self->super.super.super.deinit = grouping_by_deinit;
  self->super.super.super.clone = grouping_by_clone;
  self->super.super.process = grouping_by_process;
  g_static_mutex_init(&self->lock);
  self->scope = RCS_GLOBAL;
  self->timer_wheel = timer_wheel_new();
  timer_wheel_set_associated_data(self->timer_wheel, self, NULL);
  cached_g_current_time(&self->last_tick);
  self->timeout = -1;
  return &self->super.super;
}

void
grouping_by_global_init(void)
{
  context_id_handle = log_msg_get_value_handle(".classifier.context_id");
}
