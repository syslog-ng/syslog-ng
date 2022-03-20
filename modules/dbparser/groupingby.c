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
#include "correlation.h"
#include "correlation-context.h"
#include "synthetic-message.h"
#include "messages.h"
#include "str-utils.h"
#include "scratch-buffers.h"
#include "filter/filter-expr.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"
#include <iv.h>

typedef struct _GroupingBy
{
  StatefulParser super;
  struct iv_timer tick;
  CorrelationState *correlation;
  LogTemplate *key_template;
  LogTemplate *sort_key_template;
  gint timeout;
  gint clone_id;
  CorrelationScope scope;
  SyntheticMessage *synthetic_message;
  FilterExprNode *trigger_condition_expr;
  FilterExprNode *where_condition_expr;
  FilterExprNode *having_condition_expr;
} GroupingBy;

static NVHandle context_id_handle = 0;


#define EXPECTED_NUMBER_OF_MESSAGES_EMITTED 32
typedef struct _GPMessageEmitter
{
  gpointer emitted_messages[EXPECTED_NUMBER_OF_MESSAGES_EMITTED];
  GPtrArray *emitted_messages_overflow;
  gint num_emitted_messages;
} GPMessageEmitter;

/* public functions */

void
grouping_by_set_key_template(LogParser *s, LogTemplate *key_template)
{
  GroupingBy *self = (GroupingBy *) s;

  log_template_unref(self->key_template);
  self->key_template = log_template_ref(key_template);
}

void
grouping_by_set_sort_key_template(LogParser *s, LogTemplate *sort_key)
{
  GroupingBy *self = (GroupingBy *) s;

  log_template_unref(self->sort_key_template);
  self->sort_key_template = log_template_ref(sort_key);
}

void
grouping_by_set_scope(LogParser *s, CorrelationScope scope)
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


/* This function is called to populate the emitted_messages array in
 * msg_emitter.  It only manipulates per-thread data structure so it does
 * not require locks but does not mind them being locked either.  */
static void
_emit_message(GPMessageEmitter *msg_emitter, LogMessage *msg)
{
  if (msg_emitter->num_emitted_messages < EXPECTED_NUMBER_OF_MESSAGES_EMITTED)
    {
      msg_emitter->emitted_messages[msg_emitter->num_emitted_messages++] = log_msg_ref(msg);
      return;
    }

  if (!msg_emitter->emitted_messages_overflow)
    msg_emitter->emitted_messages_overflow = g_ptr_array_new();

  g_ptr_array_add(msg_emitter->emitted_messages_overflow, log_msg_ref(msg));
}

static void
_send_emitted_message_array(GroupingBy *self, gpointer *values, gsize len)
{
  for (gint i = 0; i < len; i++)
    {
      LogMessage *msg = values[i];
      stateful_parser_emit_synthetic(&self->super, msg);
      log_msg_unref(msg);
    }
}

/* This function is called to flush the accumulated list of messages that
 * are generated during processing.  We must not hold any locks within
 * GroupingBy when doing this, as it will cause log_pipe_queue() calls to
 * subsequent elements in the message pipeline, which in turn may recurse
 * into PatternDB.  This works as msg_emitter itself is per-thread
 * (actually an auto variable on the stack), and this is called without
 * locks held at the end of a process() invocation. */
static void
_flush_emitted_messages(GroupingBy *self, GPMessageEmitter *msg_emitter)
{
  _send_emitted_message_array(self, msg_emitter->emitted_messages, msg_emitter->num_emitted_messages);
  msg_emitter->num_emitted_messages = 0;

  if (!msg_emitter->emitted_messages_overflow)
    return;

  _send_emitted_message_array(self, msg_emitter->emitted_messages_overflow->pdata,
                              msg_emitter->emitted_messages_overflow->len);

  g_ptr_array_free(msg_emitter->emitted_messages_overflow, TRUE);
  msg_emitter->emitted_messages_overflow = NULL;

}

/* NOTE: lock should be acquired for writing before calling this function. */
void
_advance_time_based_on_message(GroupingBy *self, const UnixTime *ls, GPMessageEmitter *msg_emitter)
{
  correlation_state_set_time(self->correlation, ls->ut_sec, msg_emitter);
  msg_debug("Advancing grouping-by() current time because of an incoming message",
            evt_tag_long("utc", correlation_state_get_time(self->correlation)),
            log_pipe_location_tag(&self->super.super.super));
}

/*
 * This function can be called any time when pattern-db is not processing
 * messages, but we expect the correlation timer to move forward.  It
 * doesn't need to be called absolutely regularly as it'll use the current
 * system time to determine how much time has passed since the last
 * invocation.  See the timing comment at pattern_db_process() for more
 * information.
 */
static void
_advance_time_by_timer_tick(GroupingBy *self)
{
  GPMessageEmitter msg_emitter = {0};

  if (correlation_state_timer_tick(self->correlation, &msg_emitter))
    {
      msg_debug("Advancing grouping-by() current time because of timer tick",
                evt_tag_long("utc", correlation_state_get_time(self->correlation)),
                log_pipe_location_tag(&self->super.super.super));
    }
  _flush_emitted_messages(self, &msg_emitter);
}

static void
_timer_tick(gpointer s)
{
  GroupingBy *self = (GroupingBy *) s;

  _advance_time_by_timer_tick(self);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}

static gboolean
_evaluate_filter(FilterExprNode *expr, CorrelationContext *context)
{
  return filter_expr_eval_with_context(expr,
                                       (LogMessage **) context->messages->pdata,
                                       context->messages->len, &DEFAULT_TEMPLATE_EVAL_OPTIONS);
}

static gboolean
_evaluate_having(GroupingBy *self, CorrelationContext *context)
{
  if (!self->having_condition_expr)
    return TRUE;

  return _evaluate_filter(self->having_condition_expr, context);
}

static gboolean
_evaluate_trigger(GroupingBy *self, CorrelationContext *context)
{
  if (!self->trigger_condition_expr)
    return FALSE;

  return _evaluate_filter(self->trigger_condition_expr, context);
}

static LogMessage *
_generate_synthetic_msg(GroupingBy *self, CorrelationContext *context)
{
  LogMessage *msg = NULL;

  if (!_evaluate_having(self, context))
    {
      msg_debug("groupingby() dropping context, because having() is FALSE",
                evt_tag_str("key", context->key.session_id),
                log_pipe_location_tag(&self->super.super.super));
      return NULL;
    }

  msg = synthetic_message_generate_with_context(self->synthetic_message, context);

  return msg;
}

static LogMessage *
_aggregate_context(GroupingBy *self, CorrelationContext *context)
{
  if (self->sort_key_template)
    correlation_context_sort(context, self->sort_key_template);

  LogMessage *msg = _generate_synthetic_msg(self, context);

  correlation_state_tx_remove_context(self->correlation, context);

  /* correlation_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */

  return msg;
}

static void
_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data, gpointer caller_context)
{
  CorrelationContext *context = user_data;
  GPMessageEmitter *msg_emitter = caller_context;
  GroupingBy *self = (GroupingBy *) timer_wheel_get_associated_data(wheel);

  msg_debug("Expiring grouping-by() correlation context",
            evt_tag_long("utc", correlation_state_get_time(self->correlation)),
            evt_tag_str("context-id", context->key.session_id),
            log_pipe_location_tag(&self->super.super.super));

  context->timer = NULL;
  LogMessage *msg = _aggregate_context(self, context);
  if (msg)
    {
      _emit_message(msg_emitter, msg);
      log_msg_unref(msg);
    }
}


static CorrelationContext *
_lookup_or_create_context(GroupingBy *self, LogMessage *msg)
{
  CorrelationContext *context;
  CorrelationKey key;
  GString *buffer = scratch_buffers_alloc();

  log_template_format(self->key_template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, buffer);
  log_msg_set_value(msg, context_id_handle, buffer->str, -1);

  correlation_key_init(&key, self->scope, msg, buffer->str);
  context = correlation_state_tx_lookup_context(self->correlation, &key);
  if (!context)
    {
      msg_debug("Correlation context lookup failure, starting a new context",
                evt_tag_str("key", key.session_id),
                evt_tag_int("timeout", self->timeout),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                log_pipe_location_tag(&self->super.super.super));

      context = correlation_context_new(&key);
      correlation_state_tx_store_context(self->correlation, context, self->timeout, _expire_entry);
      g_string_steal(buffer);
    }
  else
    {
      msg_debug("Correlation context lookup successful",
                evt_tag_str("key", key.session_id),
                evt_tag_int("timeout", self->timeout),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                evt_tag_int("num_messages", context->messages->len),
                log_pipe_location_tag(&self->super.super.super));
    }

  return context;
}

static gboolean
_perform_groupby(GroupingBy *self, LogMessage *msg)
{
  GPMessageEmitter msg_emitter = {0};

  _advance_time_based_on_message(self, &msg->timestamps[LM_TS_STAMP], &msg_emitter);

  correlation_state_tx_begin(self->correlation);

  CorrelationContext *context = _lookup_or_create_context(self, msg);
  g_ptr_array_add(context->messages, log_msg_ref(msg));

  if (_evaluate_trigger(self, context))
    {
      msg_verbose("Correlation trigger() met, closing state",
                  evt_tag_str("key", context->key.session_id),
                  evt_tag_int("timeout", self->timeout),
                  evt_tag_int("num_messages", context->messages->len),
                  log_pipe_location_tag(&self->super.super.super));

      /* close down state */
      LogMessage *genmsg = _aggregate_context(self, context);

      correlation_state_tx_end(self->correlation);
      _flush_emitted_messages(self, &msg_emitter);

      if (genmsg)
        {
          stateful_parser_emit_synthetic(&self->super, genmsg);
          log_msg_unref(genmsg);
        }

      log_msg_write_protect(msg);

      return TRUE;
    }
  else
    {
      correlation_state_tx_update_context(self->correlation, context, self->timeout);
    }

  log_msg_write_protect(msg);

  correlation_state_tx_end(self->correlation);
  _flush_emitted_messages(self, &msg_emitter);

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
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const char *input,
                    gsize input_len)
{
  GroupingBy *self = (GroupingBy *) s;

  if (_evaluate_where(self, pmsg, path_options))
    return _perform_groupby(self, log_msg_make_writable(pmsg, path_options));
  return TRUE;
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  static gchar persist_name[512];
  GroupingBy *self = (GroupingBy *)s;

  g_snprintf(persist_name, sizeof(persist_name), "grouping-by(%s,scope=%d,clone=%d)", self->key_template->template, self->scope, self->clone_id);
  return persist_name;
}

static void
_load_correlation_state(GroupingBy *self, GlobalConfig *cfg)
{
  CorrelationState *persisted_correlation = cfg_persist_config_fetch(cfg,
                                                                     log_pipe_get_persist_name(&self->super.super.super));
  if (persisted_correlation)
    {
      correlation_state_unref(self->correlation);
      self->correlation = persisted_correlation;
    }

  timer_wheel_set_associated_data(self->correlation->timer_wheel, log_pipe_ref((LogPipe *)self), (GDestroyNotify)log_pipe_unref);
}

static void
_store_data_in_persist(GroupingBy *self, GlobalConfig *cfg)
{
  cfg_persist_config_add(cfg, log_pipe_get_persist_name(&self->super.super.super), correlation_state_ref(self->correlation),
                         (GDestroyNotify) correlation_state_unref, FALSE);
}

static gboolean
_init(LogPipe *s)
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

  _load_correlation_state(self, cfg);

  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = _timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);

  if (self->trigger_condition_expr && !filter_expr_init(self->trigger_condition_expr, cfg))
    return FALSE;
  if (self->where_condition_expr && !filter_expr_init(self->where_condition_expr, cfg))
    return FALSE;
  if (self->having_condition_expr && !filter_expr_init(self->having_condition_expr, cfg))
    return FALSE;

  return stateful_parser_init_method(s);
}


static gboolean
_deinit(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  _store_data_in_persist(self, cfg);

  return stateful_parser_deinit_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;
  GroupingBy *cloned;

  cloned = (GroupingBy *) grouping_by_new(s->cfg);
  grouping_by_set_key_template(&cloned->super.super, self->key_template);
  grouping_by_set_sort_key_template(&cloned->super.super, self->sort_key_template);
  grouping_by_set_timeout(&cloned->super.super, self->timeout);
  grouping_by_set_scope(&cloned->super.super, self->scope);
  grouping_by_set_synthetic_message(&cloned->super.super, self->synthetic_message);
  grouping_by_set_trigger_condition(&cloned->super.super, filter_expr_clone(self->trigger_condition_expr));
  grouping_by_set_where_condition(&cloned->super.super, filter_expr_clone(self->where_condition_expr));
  grouping_by_set_having_condition(&cloned->super.super, filter_expr_clone(self->having_condition_expr));

  correlation_state_unref(cloned->correlation);
  cloned->correlation = correlation_state_ref(self->correlation);
  cloned->clone_id = self->clone_id++;
  return &cloned->super.super.super;
}

static void
_free(LogPipe *s)
{
  GroupingBy *self = (GroupingBy *) s;

  log_template_unref(self->key_template);
  log_template_unref(self->sort_key_template);
  if (self->synthetic_message)
    synthetic_message_free(self->synthetic_message);
  stateful_parser_free_method(s);

  filter_expr_unref(self->trigger_condition_expr);
  filter_expr_unref(self->where_condition_expr);
  filter_expr_unref(self->having_condition_expr);
  correlation_state_unref(self->correlation);
}

LogParser *
grouping_by_new(GlobalConfig *cfg)
{
  GroupingBy *self = g_new0(GroupingBy, 1);

  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = _free;
  self->super.super.super.init = _init;
  self->super.super.super.deinit = _deinit;
  self->super.super.super.clone = _clone;
  self->super.super.super.generate_persist_name = _format_persist_name;
  self->super.super.process = _process;
  self->scope = RCS_GLOBAL;
  self->timeout = -1;
  self->correlation = correlation_state_new();
  return &self->super.super;
}

void
grouping_by_global_init(void)
{
  context_id_handle = log_msg_get_value_handle(".classifier.context_id");
}
