/*
 * Copyright (c) 2015 BalaBit
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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
#include "grouping-parser.h"
#include "scratch-buffers.h"
#include "str-utils.h"

void
grouping_parser_set_key_template(LogParser *s, LogTemplate *key_template)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->key_template);
  self->key_template = log_template_ref(key_template);
}

void
grouping_parser_set_sort_key_template(LogParser *s, LogTemplate *sort_key)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->sort_key_template);
  self->sort_key_template = log_template_ref(sort_key);
}

void
grouping_parser_set_scope(LogParser *s, CorrelationScope scope)
{
  GroupingParser *self = (GroupingParser *) s;

  self->scope = scope;
}

void
grouping_parser_set_timeout(LogParser *s, gint timeout)
{
  GroupingParser *self = (GroupingParser *) s;

  self->timeout = timeout;
}

void
grouping_parser_clone_settings(GroupingParser *self, GroupingParser *cloned)
{
  stateful_parser_clone_settings(&self->super, &cloned->super);
  grouping_parser_set_key_template(&cloned->super.super, self->key_template);
  grouping_parser_set_sort_key_template(&cloned->super.super, self->sort_key_template);
  grouping_parser_set_timeout(&cloned->super.super, self->timeout);
  grouping_parser_set_scope(&cloned->super.super, self->scope);
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
_advance_time_by_timer_tick(GroupingParser *self)
{
  StatefulParserEmittedMessages emitted_messages = STATEFUL_PARSER_EMITTED_MESSAGES_INIT;

  if (correlation_state_timer_tick(self->correlation, &emitted_messages))
    {
      msg_debug("grouping-parser: advancing current time because of timer tick",
                evt_tag_long("utc", correlation_state_get_time(self->correlation)),
                log_pipe_location_tag(&self->super.super.super));
    }
  stateful_parser_emitted_messages_flush(&emitted_messages, &self->super);
}

static void
_timer_tick(gpointer s)
{
  GroupingParser *self = (GroupingParser *) s;

  _advance_time_by_timer_tick(self);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}

/* NOTE: lock is acquired within correlation_state_set_time() */
void
_advance_time_based_on_message(GroupingParser *self, const UnixTime *ls,
                               StatefulParserEmittedMessages *emitted_messages)
{
  correlation_state_set_time(self->correlation, ls->ut_sec, emitted_messages);
  msg_debug("grouping-parser: advancing current time because of an incoming message",
            evt_tag_long("utc", correlation_state_get_time(self->correlation)),
            log_pipe_location_tag(&self->super.super.super));
}

static void
_load_correlation_state(GroupingParser *self, GlobalConfig *cfg)
{
  CorrelationState *persisted_correlation = cfg_persist_config_fetch(cfg,
                                            log_pipe_get_persist_name(&self->super.super.super));
  if (persisted_correlation)
    {
      correlation_state_unref(self->correlation);
      self->correlation = persisted_correlation;
    }

  timer_wheel_set_associated_data(self->correlation->timer_wheel, log_pipe_ref((LogPipe *)self),
                                  (GDestroyNotify)log_pipe_unref);
}

static void
_store_data_in_persist(GroupingParser *self, GlobalConfig *cfg)
{
  cfg_persist_config_add(cfg, log_pipe_get_persist_name(&self->super.super.super),
                         correlation_state_ref(self->correlation),
                         (GDestroyNotify) correlation_state_unref);
}


LogMessage *
grouping_parser_aggregate_context(GroupingParser *self, CorrelationContext *context)
{
  if (context->messages->len == 0)
    return NULL;

  if (self->sort_key_template)
    correlation_context_sort(context, self->sort_key_template);

  LogMessage *msg = self->aggregate_context(self, context);
  correlation_context_clear(context);

  /* correlation_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */

  return msg;
}

static void
_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data, gpointer caller_context)
{
  CorrelationContext *context = user_data;
  StatefulParserEmittedMessages *emitted_messages = caller_context;
  GroupingParser *self = (GroupingParser *) timer_wheel_get_associated_data(wheel);

  msg_debug("grouping-parser: Expiring correlation context",
            evt_tag_long("utc", correlation_state_get_time(self->correlation)),
            evt_tag_str("context-id", context->key.session_id),
            log_pipe_location_tag(&self->super.super.super));

  context->timer = NULL;
  LogMessage *msg = grouping_parser_aggregate_context(self, context);
  correlation_state_tx_remove_context(self->correlation, context);

  if (msg)
    {
      stateful_parser_emitted_messages_add(emitted_messages, msg);
      log_msg_unref(msg);
    }
}


CorrelationContext *
grouping_parser_lookup_or_create_context(GroupingParser *self, LogMessage *msg)
{
  CorrelationContext *context;
  CorrelationKey key;
  GString *buffer = scratch_buffers_alloc();

  log_template_format(self->key_template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, buffer);

  correlation_key_init(&key, self->scope, msg, buffer->str);
  context = correlation_state_tx_lookup_context(self->correlation, &key);
  if (!context)
    {
      msg_debug("grouping-parser: Correlation context lookup failure, starting a new context",
                evt_tag_str("key", key.session_id),
                evt_tag_int("timeout", self->timeout),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                log_pipe_location_tag(&self->super.super.super));

      context = grouping_parser_construct_context(self, &key);
      correlation_state_tx_store_context(self->correlation, context, self->timeout);
      g_string_steal(buffer);
    }
  else
    {
      msg_debug("grouping-parser: Correlation context lookup successful",
                evt_tag_str("key", key.session_id),
                evt_tag_int("timeout", self->timeout),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                evt_tag_int("num_messages", context->messages->len),
                log_pipe_location_tag(&self->super.super.super));
    }

  return context;
}

static void
_aggregate_and_emit(GroupingParser *self, CorrelationContext *context, StatefulParserEmittedMessages *emitted_messages)
{
  LogMessage *genmsg = grouping_parser_aggregate_context(self, context);
  correlation_state_tx_update_context(self->correlation, context, self->timeout);
  correlation_state_tx_end(self->correlation);
  if (genmsg)
    {
      stateful_parser_emitted_messages_add(emitted_messages, genmsg);
      log_msg_unref(genmsg);
    }
}

void
grouping_parser_perform_grouping(GroupingParser *self, LogMessage *msg, StatefulParserEmittedMessages *emitted_messages)
{
  correlation_state_tx_begin(self->correlation);

  CorrelationContext *context = grouping_parser_lookup_or_create_context(self, msg);

  GroupingParserUpdateContextResult r = grouping_parser_update_context(self, context, msg);

  if (r == GP_CONTEXT_UPDATED)
    {
      msg_debug("grouping-parser: Correlation context update successful",
                evt_tag_str("key", context->key.session_id),
                evt_tag_int("num_messages", context->messages->len),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                log_pipe_location_tag(&self->super.super.super));
      correlation_state_tx_update_context(self->correlation, context, self->timeout);
      correlation_state_tx_end(self->correlation);
    }
  else if (r == GP_CONTEXT_COMPLETE)
    {
      msg_debug("grouping-parser: Correlation finished, aggregating context",
                evt_tag_str("key", context->key.session_id),
                evt_tag_int("num_messages", context->messages->len),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                log_pipe_location_tag(&self->super.super.super));
      _aggregate_and_emit(self, context, emitted_messages);
    }
  else if (r == GP_STARTS_NEW_CONTEXT)
    {
      msg_debug("grouping-parser: Correlation finished, aggregating and starting a new context",
                evt_tag_str("key", context->key.session_id),
                evt_tag_int("num_messages", context->messages->len),
                evt_tag_int("expiration", correlation_state_get_time(self->correlation) + self->timeout),
                log_pipe_location_tag(&self->super.super.super));
      _aggregate_and_emit(self, context, emitted_messages);
      grouping_parser_perform_grouping(self, msg, emitted_messages);
    }
}

gboolean
grouping_parser_process_method(LogParser *s,
                               LogMessage **pmsg, const LogPathOptions *path_options,
                               const char *input, gsize input_len)
{
  GroupingParser *self = (GroupingParser *) s;

  if (grouping_parser_filter_messages(self, pmsg, path_options))
    {
      LogMessage *msg = *pmsg;

      StatefulParserEmittedMessages emitted_messages = STATEFUL_PARSER_EMITTED_MESSAGES_INIT;
      _advance_time_based_on_message(self, &msg->timestamps[LM_TS_STAMP], &emitted_messages);

      grouping_parser_perform_grouping(self, msg, &emitted_messages);
      stateful_parser_emitted_messages_flush(&emitted_messages, &self->super);
    }
  return (self->super.inject_mode != LDBP_IM_AGGREGATE_ONLY);
}

gboolean
grouping_parser_init_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = _timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);

  _load_correlation_state(self, cfg);

  return stateful_parser_init_method(s);
}

gboolean
grouping_parser_deinit_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  _store_data_in_persist(self, cfg);
  return stateful_parser_deinit_method(s);
}

void
grouping_parser_free_method(LogPipe *s)
{
  GroupingParser *self = (GroupingParser *) s;

  log_template_unref(self->key_template);
  log_template_unref(self->sort_key_template);
  correlation_state_unref(self->correlation);

  stateful_parser_free_method(s);
}

void
grouping_parser_init_instance(GroupingParser *self, GlobalConfig *cfg)
{
  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = grouping_parser_free_method;
  self->super.super.super.init = grouping_parser_init_method;
  self->super.super.super.deinit = grouping_parser_deinit_method;
  self->super.super.process = grouping_parser_process_method;
  self->scope = RCS_GLOBAL;
  self->timeout = -1;
  self->correlation = correlation_state_new(_expire_entry);
}

void
grouping_parser_global_init(void)
{
}
