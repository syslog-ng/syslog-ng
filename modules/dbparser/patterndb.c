/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "patterndb.h"
#include "pdb-action.h"
#include "pdb-rule.h"
#include "pdb-program.h"
#include "pdb-ruleset.h"
#include "pdb-load.h"
#include "pdb-context.h"
#include "pdb-ratelimit.h"
#include "pdb-lookup-params.h"
#include "correllation.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "str-utils.h"
#include "filter/filter-expr-parser.h"
#include "logpipe.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static NVHandle context_id_handle = 0;


struct _PatternDB
{
  GStaticRWLock lock;
  PDBRuleSet *ruleset;
  CorrellationState correllation;
  GHashTable *rate_limits;
  TimerWheel *timer_wheel;
  GTimeVal last_tick;
  PatternDBEmitFunc emit;
  gpointer emit_data;
};

typedef struct _PatternDBProcessState
{
  PDBRule *rule;
  PDBAction *action;
  PDBContext *context;
  LogMessage *msg;
} PatternDBProcessState;

/*
 * Timing
 * ======
 *
 * The time tries to follow the message stream, e.g. it is independent from
 * the current system time.  Whenever a message comes in, its timestamp
 * moves the current time forward, which means it is quite easy to process
 * logs from the past, correllation timeouts will be measured in "message
 * time".  There's one exception to this rule: when the patterndb is idle
 * (e.g.  no messages are coming in), the current system time is used to
 * measure as real time passes, and that will also increase the time of the
 * correllation engine. This is based on the following assumptions:
 *
 *    1) dbparser can only be idle in case on-line logs are processed
 *       (otherwise messages are read from the disk much faster)
 *
 *    2) if on-line processing is done, it is expected that messages have
 *       roughly correct timestamps, e.g. if 1 second passes in current
 *       system time, further incoming messages will have a timestamp close
 *       to this.
 *
 * Thus whenever the patterndb is idle, a timer tick callback arrives, which
 * checks the real elapsed time between the last message (or last tick) and
 * increments the current known time with this value.
 *
 * This behaviour makes it possible to properly work in these use-cases:
 *
 *    1) process a log file stored on disk, containing messages in the past
 *    2) process an incoming message stream on-line, expiring correllation
 *    states even if there are no incoming messages
 *
 */


/*********************************************
 * Rule evaluation
 *********************************************/

static gboolean
_is_action_within_rate_limit(PatternDB *db, PDBRule *rule, PDBAction *action, LogMessage *msg, GString *buffer)
{
  CorrellationKey key;
  PDBRateLimit *rl;
  guint64 now;

  if (action->rate == 0)
    return TRUE;

  g_string_printf(buffer, "%s:%d", rule->rule_id, action->id);
  correllation_key_setup(&key, rule->context.scope, msg, buffer->str);

  rl = g_hash_table_lookup(db->rate_limits, &key);
  if (!rl)
    {
      rl = pdb_rate_limit_new(&key);
      g_hash_table_insert(db->rate_limits, &rl->key, rl);
      g_string_steal(buffer);
    }
  now = timer_wheel_get_time(db->timer_wheel);
  if (rl->last_check == 0)
    {
      rl->last_check = now;
      rl->buckets = action->rate;
    }
  else
    {
      /* quick and dirty fixed point arithmetic, 8 bit fraction part */
      gint new_credits = (((glong) (now - rl->last_check)) << 8) / ((((glong) action->rate_quantum) << 8) / action->rate);

      if (new_credits)
        {
          /* ok, enough time has passed to increase the current credit.
           * Deposit the new credits in bucket but make sure we don't permit
           * more than the maximum rate. */

          rl->buckets = MIN(rl->buckets + new_credits, action->rate);
          rl->last_check = now;
        }
    }
  if (rl->buckets)
    {
      rl->buckets--;
      return TRUE;
    }
  return FALSE;
}

static gboolean
_is_action_triggered(PatternDB *db, PDBRule *rule, PDBAction *action, PDBActionTrigger trigger, PDBContext *context,
                     LogMessage *msg, GString *buffer)
{
  if (action->trigger != trigger)
    return FALSE;

  if (action->condition)
    {
      if (context
          && !filter_expr_eval_with_context(action->condition, (LogMessage **) context->super.messages->pdata,
                                            context->super.messages->len))
        return FALSE;
      if (!context && !filter_expr_eval(action->condition, msg))
        return FALSE;
    }

  if (!_is_action_within_rate_limit(db, rule, action, msg, buffer))
    return FALSE;

  return TRUE;
}

static LogMessage *
_generate_synthetic_message(PDBAction *action, PDBContext *context, LogMessage *msg, GString *buffer)
{
  if (context)
    return synthetic_message_generate_with_context(&action->content.message, &context->super, buffer);
  else
    return synthetic_message_generate_without_context(&action->content.message, msg, buffer);
}

static void
_execute_action_message(PatternDB *db, PDBAction *action, PDBContext *context, LogMessage *msg, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = _generate_synthetic_message(action, context, msg, buffer);
  db->emit(genmsg, TRUE, db->emit_data);
  log_msg_unref(genmsg);
}

static void pattern_db_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data);

static void
_execute_action_create_context(PatternDB *db, PDBAction *action, PDBRule *rule, PDBContext *triggering_context,
                               LogMessage *triggering_msg, GString *buffer)
{
  CorrellationKey key;
  PDBContext *new_context;
  LogMessage *context_msg;
  SyntheticContext *syn_context;
  SyntheticMessage *syn_message;

  syn_context = &action->content.create_context.context;
  syn_message = &action->content.create_context.message;
  if (triggering_context)
    {
      context_msg = synthetic_message_generate_with_context(syn_message, &triggering_context->super, buffer);
      log_template_format_with_context(syn_context->id_template,
                                       (LogMessage **) triggering_context->super.messages->pdata, triggering_context->super.messages->len,
                                       NULL, LTZ_LOCAL, 0, NULL, buffer);
    }
  else
    {
      context_msg = synthetic_message_generate_without_context(syn_message, triggering_msg, buffer);
      log_template_format(syn_context->id_template,
                          triggering_msg,
                          NULL, LTZ_LOCAL, 0, NULL, buffer);
    }

  msg_debug("Explicit create-context action, starting a new context",
            evt_tag_str("rule", rule->rule_id),
            evt_tag_str("context", buffer->str),
            evt_tag_int("context_timeout", syn_context->timeout),
            evt_tag_int("context_expiration", timer_wheel_get_time(db->timer_wheel) + syn_context->timeout));

  correllation_key_setup(&key, syn_context->scope, context_msg, buffer->str);
  new_context = pdb_context_new(&key);
  g_hash_table_insert(db->correllation.state, &new_context->super.key, new_context);
  g_string_steal(buffer);

  g_ptr_array_add(new_context->super.messages, context_msg);

  new_context->super.timer = timer_wheel_add_timer(db->timer_wheel, rule->context.timeout, pattern_db_expire_entry,
                                                   correllation_context_ref(&new_context->super),
                                                   (GDestroyNotify) correllation_context_unref);
  new_context->rule = pdb_rule_ref(rule);
}

static void
_execute_action(PatternDB *db, PDBRule *rule, PDBAction *action, PDBContext *context, LogMessage *msg, GString *buffer)
{
  switch (action->content_type)
    {
    case RAC_NONE:
      break;
    case RAC_MESSAGE:
      _execute_action_message(db, action, context, msg, buffer);
      break;
    case RAC_CREATE_CONTEXT:
      _execute_action_create_context(db, action, rule, context, msg, buffer);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

static void
_execute_action_if_triggered(PatternDB *db, PDBRule *rule, PDBAction *action,
                             PDBActionTrigger trigger, PDBContext *context,
                             LogMessage *msg, GString *buffer)
{
  if (_is_action_triggered(db, rule, action, trigger, context, msg, buffer))
    _execute_action(db, rule, action, context, msg, buffer);
}

static void
_execute_rule_actions(PatternDB *db, PDBRule *rule,
                      PDBActionTrigger trigger, PDBContext *context,
                      LogMessage *msg, GString *buffer)
{
  gint i;

  if (!rule->actions)
    return;
  for (i = 0; i < rule->actions->len; i++)
    {
      PDBAction *action = (PDBAction *) g_ptr_array_index(rule->actions, i);

      _execute_action_if_triggered(db, rule, action, trigger, context, msg, buffer);
    }
}

/*********************************************************
 * PatternDB
 *********************************************************/

/* NOTE: this function requires PatternDB reader/writer lock to be
 * write-locked.
 *
 * Currently, it is, as timer_wheel_set_time() is only called with that
 * precondition, and timer-wheel callbacks are only called from within
 * timer_wheel_set_time().
 */

static void
pattern_db_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data)
{
  PDBContext *context = user_data;
  PatternDB *pdb = (PatternDB *) timer_wheel_get_associated_data(wheel);
  GString *buffer = g_string_sized_new(256);
  LogMessage *msg = correllation_context_get_last_message(&context->super);

  msg_debug("Expiring patterndb correllation context",
            evt_tag_str("last_rule", context->rule->rule_id),
            evt_tag_long("utc", timer_wheel_get_time(pdb->timer_wheel)));
  if (pdb->emit)
    _execute_rule_actions(pdb, context->rule, RAT_TIMEOUT, context, msg, buffer);
  g_hash_table_remove(pdb->correllation.state, &context->super.key);
  g_string_free(buffer, TRUE);

  /* pdb_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */
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
pattern_db_timer_tick(PatternDB *self)
{
  GTimeVal now;
  glong diff;

  g_static_rw_lock_writer_lock(&self->lock);
  cached_g_current_time(&now);
  diff = g_time_val_diff(&now, &self->last_tick);

  if (diff > 1e6)
    {
      glong diff_sec = diff / 1e6;

      timer_wheel_set_time(self->timer_wheel, timer_wheel_get_time(self->timer_wheel) + diff_sec);
      msg_debug("Advancing patterndb current time because of timer tick",
                evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)));
      /* update last_tick, take the fraction of the seconds not calculated into this update into account */

      self->last_tick = now;
      g_time_val_add(&self->last_tick, -(diff - diff_sec * 1e6));
    }
  else if (diff < 0)
    {
      /* time moving backwards, this can only happen if the computer's time
       * is changed.  We don't update patterndb's idea of the time now, wait
       * another tick instead to update that instead.
       */
      self->last_tick = now;
    }
  g_static_rw_lock_writer_unlock(&self->lock);
}

/* NOTE: lock should be acquired for writing before calling this function. */
void
pattern_db_set_time(PatternDB *self, const LogStamp *ls)
{
  GTimeVal now;

  /* clamp the current time between the timestamp of the current message
   * (low limit) and the current system time (high limit).  This ensures
   * that incorrect clocks do not skew the current time know by the
   * correllation engine too much. */

  cached_g_current_time(&now);
  self->last_tick = now;

  if (ls->tv_sec < now.tv_sec)
    now.tv_sec = ls->tv_sec;

  timer_wheel_set_time(self->timer_wheel, now.tv_sec);
  msg_debug("Advancing patterndb current time because of an incoming message",
            evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)));
}

gboolean
pattern_db_reload_ruleset(PatternDB *self, GlobalConfig *cfg, const gchar *pdb_file)
{
  PDBRuleSet *new_ruleset;

  new_ruleset = pdb_rule_set_new();
  if (!pdb_rule_set_load(new_ruleset, cfg, pdb_file, NULL))
    {
      pdb_rule_set_free(new_ruleset);
      return FALSE;
    }
  else
    {
      g_static_rw_lock_writer_lock(&self->lock);
      if (self->ruleset)
        pdb_rule_set_free(self->ruleset);
      self->ruleset = new_ruleset;
      g_static_rw_lock_writer_unlock(&self->lock);
      return TRUE;
    }
}


void
pattern_db_set_emit_func(PatternDB *self, PatternDBEmitFunc emit, gpointer emit_data)
{
  self->emit = emit;
  self->emit_data = emit_data;
}

const gchar *
pattern_db_get_ruleset_pub_date(PatternDB *self)
{
  return self->ruleset->pub_date;
}

const gchar *
pattern_db_get_ruleset_version(PatternDB *self)
{
  return self->ruleset->version;
}

PDBRuleSet *
pattern_db_get_ruleset(PatternDB *self)
{
  return self->ruleset;
}

TimerWheel *
pattern_db_get_timer_wheel(PatternDB *self)
{
  return self->timer_wheel;
}

static gboolean
_pattern_db_is_empty(PatternDB *self)
{
  return (G_UNLIKELY(!self->ruleset) || self->ruleset->is_empty);
}

static void
_pattern_db_process_matching_rule(PatternDB *self, PatternDBProcessState *process_state)
{
  PDBContext *context = NULL;
  PDBRule *rule = process_state->rule;
  LogMessage *msg = process_state->msg;
  GString *buffer = g_string_sized_new(32);

  g_static_rw_lock_writer_lock(&self->lock);
  pattern_db_set_time(self, &msg->timestamps[LM_TS_STAMP]);
  if (rule->context.id_template)
    {
      CorrellationKey key;

      log_template_format(rule->context.id_template, msg, NULL, LTZ_LOCAL, 0, NULL, buffer);
      log_msg_set_value(msg, context_id_handle, buffer->str, -1);

      correllation_key_setup(&key, rule->context.scope, msg, buffer->str);
      context = g_hash_table_lookup(self->correllation.state, &key);
      if (!context)
        {
          msg_debug("Correllation context lookup failure, starting a new context",
                    evt_tag_str("rule", rule->rule_id),
                    evt_tag_str("context", buffer->str),
                    evt_tag_int("context_timeout", rule->context.timeout),
                    evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + rule->context.timeout));
          context = pdb_context_new(&key);
          g_hash_table_insert(self->correllation.state, &context->super.key, context);
          g_string_steal(buffer);
        }
      else
        {
          msg_debug("Correllation context lookup successful",
                    evt_tag_str("rule", rule->rule_id),
                    evt_tag_str("context", buffer->str),
                    evt_tag_int("context_timeout", rule->context.timeout),
                    evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + rule->context.timeout),
                    evt_tag_int("num_messages", context->super.messages->len));
        }

      g_ptr_array_add(context->super.messages, log_msg_ref(msg));

      if (context->super.timer)
        {
          timer_wheel_mod_timer(self->timer_wheel, context->super.timer, rule->context.timeout);
        }
      else
        {
          context->super.timer = timer_wheel_add_timer(self->timer_wheel, rule->context.timeout, pattern_db_expire_entry,
                                                       correllation_context_ref(&context->super),
                                                       (GDestroyNotify) correllation_context_unref);
        }
      if (context->rule != rule)
        {
          if (context->rule)
            pdb_rule_unref(context->rule);
          context->rule = pdb_rule_ref(rule);
        }
    }
  else
    {
      context = NULL;
    }

  process_state->context = context;
  synthetic_message_apply(&rule->msg, &context->super, msg, buffer);
  if (self->emit)
    {
      g_static_rw_lock_writer_unlock(&self->lock);
      self->emit(msg, FALSE, self->emit_data);
      _execute_rule_actions(self, rule, RAT_MATCH, context, msg, buffer);
      g_static_rw_lock_writer_lock(&self->lock);
    }
  pdb_rule_unref(rule);
  g_static_rw_lock_writer_unlock(&self->lock);

  if (context)
    log_msg_write_protect(msg);

  g_string_free(buffer, TRUE);
}

static void
_pattern_db_process_unmatching_rule(PatternDB *self, PatternDBProcessState *process_state)
{
  LogMessage *msg = process_state->msg;

  g_static_rw_lock_writer_lock(&self->lock);
  pattern_db_set_time(self, &msg->timestamps[LM_TS_STAMP]);
  g_static_rw_lock_writer_unlock(&self->lock);
  if (self->emit)
    self->emit(msg, FALSE, self->emit_data);
}

static gboolean
_pattern_db_process(PatternDB *self, PDBLookupParams *lookup, GArray *dbg_list)
{
  LogMessage *msg = lookup->msg;
  PatternDBProcessState process_state_p = {0};
  PatternDBProcessState *process_state = &process_state_p;

  g_static_rw_lock_reader_lock(&self->lock);
  if (_pattern_db_is_empty(self))
    {
      g_static_rw_lock_reader_unlock(&self->lock);
      return FALSE;
    }
  process_state->rule = pdb_ruleset_lookup(self->ruleset, lookup, dbg_list);
  process_state->msg = msg;
  g_static_rw_lock_reader_unlock(&self->lock);
  if (process_state->rule)
    _pattern_db_process_matching_rule(self, process_state);
  else
    _pattern_db_process_unmatching_rule(self, process_state);
  return process_state->rule != NULL;
}


gboolean
pattern_db_process(PatternDB *self, LogMessage *msg)
{
  PDBLookupParams lookup;

  pdb_lookup_params_init(&lookup, msg);
  return _pattern_db_process(self, &lookup, NULL);
}

gboolean
pattern_db_process_with_custom_message(PatternDB *self, LogMessage *msg, const gchar *message, gssize message_len)
{
  PDBLookupParams lookup;

  pdb_lookup_params_init(&lookup, msg);
  lookup.message_handle = LM_V_NONE;
  lookup.message_string = message;
  lookup.message_len = message_len;
  return _pattern_db_process(self, &lookup, NULL);
}

void
pattern_db_debug_ruleset(PatternDB *self, LogMessage *msg, GArray *dbg_list)
{
  PDBLookupParams lookup;

  pdb_lookup_params_init(&lookup, msg);
  _pattern_db_process(self, &lookup, dbg_list);
}

void
pattern_db_expire_state(PatternDB *self)
{
  g_static_rw_lock_writer_lock(&self->lock);
  timer_wheel_expire_all(self->timer_wheel);
  g_static_rw_lock_writer_unlock(&self->lock);
}

static void
_init_state(PatternDB *self)
{
  self->rate_limits = g_hash_table_new_full(correllation_key_hash, correllation_key_equal, NULL,
                                            (GDestroyNotify) pdb_rate_limit_free);
  correllation_state_init_instance(&self->correllation);
  self->timer_wheel = timer_wheel_new();
  timer_wheel_set_associated_data(self->timer_wheel, self, NULL);
}

static void
_destroy_state(PatternDB *self)
{
  if (self->timer_wheel)
    timer_wheel_free(self->timer_wheel);

  g_hash_table_destroy(self->rate_limits);
  correllation_state_deinit_instance(&self->correllation);
}

void
pattern_db_forget_state(PatternDB *self)
{
  g_static_rw_lock_writer_lock(&self->lock);
  _destroy_state(self);
  _init_state(self);
  g_static_rw_lock_writer_unlock(&self->lock);
}

PatternDB *
pattern_db_new(void)
{
  PatternDB *self = g_new0(PatternDB, 1);

  self->ruleset = pdb_rule_set_new();
  _init_state(self);
  cached_g_current_time(&self->last_tick);
  g_static_rw_lock_init(&self->lock);
  return self;
}

void
pattern_db_free(PatternDB *self)
{
  if (self->ruleset)
    pdb_rule_set_free(self->ruleset);
  _destroy_state(self);
  g_static_rw_lock_free(&self->lock);
  g_free(self);
}

void
pattern_db_global_init(void)
{
  context_id_handle = log_msg_get_value_handle(".classifier.context_id");
  pdb_rule_set_global_init();
}
