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

static NVHandle class_handle = 0;
static NVHandle rule_id_handle = 0;
static NVHandle context_id_handle = 0;
static LogTagId system_tag;
static LogTagId unknown_tag;

typedef struct _PDBLookupParams PDBLookupParams;
struct _PDBLookupParams
{
  LogMessage *msg;
  NVHandle program_handle;
  NVHandle message_handle;
  const gchar *message_string;
  gssize message_len;
};

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


/**************************************************************************
 * PDBContext, represents a correllation state in the state hash table, is
 * marked with PSK_CONTEXT in the hash table key
 **************************************************************************/

/* This class encapsulates a correllation context, keyed by CorrellationKey, type == PSK_RULE. */
typedef struct _PDBContext
{
  CorrellationContext super;
  /* back reference to the last rule touching this context */
  PDBRule *rule;
} PDBContext;

static void
pdb_context_free(CorrellationContext *s)
{
  PDBContext *self = (PDBContext *) s;

  if (self->rule)
    pdb_rule_unref(self->rule);
  correllation_context_free_method(s);
}

PDBContext *
pdb_context_new(CorrellationKey *key)
{
  PDBContext *self = g_new0(PDBContext, 1);

  correllation_context_init(&self->super, key);
  self->super.free_fn = pdb_context_free;
  return self;
}

/***************************************************************************
 * PDBRateLimit
 ***************************************************************************/

/* This class encapsulates a rate-limit state stored in
   db->state. */
typedef struct _PDBRateLimit
{
  /* key in the hashtable. NOTE: host/program/pid/session_id are allocated, thus they need to be freed when the structure is freed. */
  CorrellationKey key;
  gint buckets;
  guint64 last_check;
} PDBRateLimit;

PDBRateLimit *
pdb_rate_limit_new(CorrellationKey *key)
{
  PDBRateLimit *self = g_new0(PDBRateLimit, 1);

  memcpy(&self->key, key, sizeof(*key));
  if (self->key.pid)
    self->key.pid = g_strdup(self->key.pid);
  if (self->key.program)
    self->key.program = g_strdup(self->key.program);
  if (self->key.host)
    self->key.host = g_strdup(self->key.host);
  return self;
}

void
pdb_rate_limit_free(PDBRateLimit *self)
{
  if (self->key.host)
    g_free((gchar *) self->key.host);
  if (self->key.program)
    g_free((gchar *) self->key.program);
  if (self->key.pid)
    g_free((gchar *) self->key.pid);
  g_free(self->key.session_id);
  g_free(self);
}

/*********************************************
 * Rule evaluation
 *********************************************/

static inline gboolean
pdb_check_action_rate_limit(PDBAction *self, PDBRule *rule, PatternDB *db, LogMessage *msg, GString *buffer)
{
  CorrellationKey key;
  PDBRateLimit *rl;
  guint64 now;

  if (self->rate == 0)
    return TRUE;

  g_string_printf(buffer, "%s:%d", rule->rule_id, self->id);
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
      rl->buckets = self->rate;
    }
  else
    {
      /* quick and dirty fixed point arithmetic, 8 bit fraction part */
      gint new_credits = (((glong) (now - rl->last_check)) << 8) / ((((glong) self->rate_quantum) << 8) / self->rate);

      if (new_credits)
        {
          /* ok, enough time has passed to increase the current credit.
           * Deposit the new credits in bucket but make sure we don't permit
           * more than the maximum rate. */

          rl->buckets = MIN(rl->buckets + new_credits, self->rate);
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

gboolean
pdb_is_action_triggered(PDBAction *self, PatternDB *db, PDBRule *rule, PDBActionTrigger trigger, PDBContext *context, LogMessage *msg, GString *buffer)
{
  if (self->trigger != trigger)
    return FALSE;

  if (self->condition)
    {
      if (context && !filter_expr_eval_with_context(self->condition, (LogMessage **) context->super.messages->pdata, context->super.messages->len))
        return FALSE;
      if (!context && !filter_expr_eval(self->condition, msg))
        return FALSE;
    }

  if (!pdb_check_action_rate_limit(self, rule, db, msg, buffer))
    return FALSE;

  return TRUE;
}

LogMessage *
pdb_generate_message(PDBAction *self, PDBContext *context, LogMessage *msg, GString *buffer)
{
  if (context)
    return synthetic_message_generate_with_context(&self->content.message, &context->super, buffer);
  else
    return synthetic_message_generate_without_context(&self->content.message, msg, buffer);
}

void
pdb_execute_action_message(PDBAction *self, PatternDB *db, PDBContext *context, LogMessage *msg, GString *buffer)
{
  LogMessage *genmsg;

  genmsg = pdb_generate_message(self, context, msg, buffer);
  db->emit(genmsg, TRUE, db->emit_data);
  log_msg_unref(genmsg);
}

static void pattern_db_expire_entry(TimerWheel *wheel, guint64 now, gpointer user_data);

void
pdb_execute_action_create_context(PDBAction *self, PatternDB *db, PDBRule *rule, PDBContext *triggering_context, LogMessage *triggering_msg, GString *buffer)
{
  CorrellationKey key;
  PDBContext *new_context;
  LogMessage *context_msg;
  SyntheticContext *syn_context;
  SyntheticMessage *syn_message;

  syn_context = &self->content.create_context.context;
  syn_message = &self->content.create_context.message;
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

void
pdb_execute_action(PDBAction *self, PatternDB *db, PDBRule *rule, PDBContext *context, LogMessage *msg, GString *buffer)
{
  switch (self->content_type)
    {
    case RAC_NONE:
      break;
    case RAC_MESSAGE:
      pdb_execute_action_message(self, db, context, msg, buffer);
      break;
    case RAC_CREATE_CONTEXT:
      pdb_execute_action_create_context(self, db, rule, context, msg, buffer);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

void
pdb_trigger_action(PDBAction *self, PatternDB *db, PDBRule *rule, PDBActionTrigger trigger, PDBContext *context, LogMessage *msg, GString *buffer)
{
  if (pdb_is_action_triggered(self, db, rule, trigger, context, msg, buffer))
    pdb_execute_action(self, db, rule, context, msg, buffer);
}

void
pdb_run_rule_actions(PDBRule *self, PatternDB *db, PDBActionTrigger trigger, PDBContext *context, LogMessage *msg, GString *buffer)
{
  gint i;

  if (!self->actions)
    return;
  for (i = 0; i < self->actions->len; i++)
    {
      PDBAction *action = (PDBAction *) g_ptr_array_index(self->actions, i);

      pdb_trigger_action(action, db, self, trigger, context, msg, buffer);
    }
}

/**
 * _add_matches_to_message:
 *
 * Adds the values from the given GArray of RParserMatch entries to the NVTable
 * of the passed LogMessage.
 *
 * @msg: the LogMessage to add the matches to
 * @matches: an array of RParserMatch entries
 * @ref_handle: if the matches are indirect matches, they are referenced based on this handle (eg. LM_V_MESSAGE)
 **/
void
_add_matches_to_message(LogMessage *msg, GArray *matches, NVHandle ref_handle, const gchar *input_string)
{
  gint i;
  for (i = 0; i < matches->len; i++)
    {
      RParserMatch *match = &g_array_index(matches, RParserMatch, i);

      if (match->match)
        {
          log_msg_set_value(msg, match->handle, match->match, match->len);
          g_free(match->match);
        }
      else if (ref_handle != LM_V_NONE && log_msg_is_handle_settable_with_an_indirect_value(match->handle))
        {
          log_msg_set_value_indirect(msg, match->handle, ref_handle, match->type, match->ofs, match->len);
        }
      else
        {
          log_msg_set_value(msg, match->handle, input_string + match->ofs, match->len);
        }
    }
}

/*
 * Looks up a matching rule in the ruleset.
 *
 * NOTE: it also modifies @msg to store the name-value pairs found during lookup, so
 */
PDBRule *
pdb_lookup_ruleset(PDBRuleSet *self, PDBLookupParams *lookup, GArray *dbg_list)
{
  RNode *node;
  LogMessage *msg = lookup->msg;
  GArray *prg_matches, *matches;
  const gchar *program_value;
  gssize program_len;

  if (G_UNLIKELY(!self->programs))
    return FALSE;

  program_value = log_msg_get_value(msg, lookup->program_handle, &program_len);
  prg_matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
  node = r_find_node(self->programs, (guint8 *) program_value, program_len, prg_matches);

  if (node)
    {
      _add_matches_to_message(msg, prg_matches, lookup->program_handle, program_value);
      g_array_free(prg_matches, TRUE);

      PDBProgram *program = (PDBProgram *) node->value;

      if (program->rules)
        {
          RNode *msg_node;
          const gchar *message;
          gssize message_len;

          /* NOTE: We're not using g_array_sized_new as that does not
           * correctly zero-initialize the new items even if clear_ is TRUE
           */

          matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
          g_array_set_size(matches, 1);

          if (lookup->message_handle)
            {
              message = log_msg_get_value(msg, lookup->message_handle, &message_len);
            }
          else
            {
              message = lookup->message_string;
              message_len = lookup->message_len;
            }

          if (G_UNLIKELY(dbg_list))
            msg_node = r_find_node_dbg(program->rules, (guint8 *) message, message_len, matches, dbg_list);
          else
            msg_node = r_find_node(program->rules, (guint8 *) message, message_len, matches);

          if (msg_node)
            {
              PDBRule *rule = (PDBRule *) msg_node->value;
              GString *buffer = g_string_sized_new(32);

              msg_debug("patterndb rule matches",
                        evt_tag_str("rule_id", rule->rule_id));
              log_msg_set_value(msg, class_handle, rule->class ? rule->class : "system", -1);
              log_msg_set_value(msg, rule_id_handle, rule->rule_id, -1);

              _add_matches_to_message(msg, matches, lookup->message_handle, message);
              g_array_free(matches, TRUE);

              if (!rule->class)
                {
                  log_msg_set_tag_by_id(msg, system_tag);
                }
              log_msg_clear_tag_by_id(msg, unknown_tag);
              g_string_free(buffer, TRUE);
              pdb_rule_ref(rule);
              return rule;
            }
          else
            {
              log_msg_set_value(msg, class_handle, "unknown", 7);
              log_msg_set_tag_by_id(msg, unknown_tag);
            }
          g_array_free(matches, TRUE);
        }
    }
  else
    {
      g_array_free(prg_matches, TRUE);
    }

  return NULL;

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
    pdb_run_rule_actions(context->rule, pdb, RAT_TIMEOUT, context, msg, buffer);
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
_pattern_db_process_matching_rule(PatternDB *self, PDBRule *rule, LogMessage *msg)
{
  PDBContext *context = NULL;
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

  synthetic_message_apply(&rule->msg, &context->super, msg, buffer);
  if (self->emit)
    {
      g_static_rw_lock_writer_unlock(&self->lock);
      self->emit(msg, FALSE, self->emit_data);
      pdb_run_rule_actions(rule, self, RAT_MATCH, context, msg, buffer);
      g_static_rw_lock_writer_lock(&self->lock);
    }
  pdb_rule_unref(rule);
  g_static_rw_lock_writer_unlock(&self->lock);

  if (context)
    log_msg_write_protect(msg);

  g_string_free(buffer, TRUE);
}

static void
_pattern_db_process_unmatching_rule(PatternDB *self, LogMessage *msg)
{
  g_static_rw_lock_writer_lock(&self->lock);
  pattern_db_set_time(self, &msg->timestamps[LM_TS_STAMP]);
  g_static_rw_lock_writer_unlock(&self->lock);
  if (self->emit)
    self->emit(msg, FALSE, self->emit_data);
}

static gboolean
_pattern_db_process(PatternDB *self, PDBLookupParams *lookup, GArray *dbg_list)
{
  PDBRule *rule;
  LogMessage *msg = lookup->msg;

  g_static_rw_lock_reader_lock(&self->lock);
  if (_pattern_db_is_empty(self))
    {
      g_static_rw_lock_reader_unlock(&self->lock);
      return FALSE;
    }
  rule = pdb_lookup_ruleset(self->ruleset, lookup, dbg_list);
  g_static_rw_lock_reader_unlock(&self->lock);
  if (rule)
    _pattern_db_process_matching_rule(self, rule, msg);
  else
    _pattern_db_process_unmatching_rule(self, msg);
  return rule != NULL;
}

static void
pdb_lookup_state_init(PDBLookupParams *lookup, LogMessage *msg)
{
  lookup->msg = msg;
  lookup->program_handle = LM_V_PROGRAM;
  lookup->message_handle = LM_V_MESSAGE;
  lookup->message_len = 0;
}

gboolean
pattern_db_process(PatternDB *self, LogMessage *msg)
{
  PDBLookupParams lookup;

  pdb_lookup_state_init(&lookup, msg);
  return _pattern_db_process(self, &lookup, NULL);
}

gboolean
pattern_db_process_with_custom_message(PatternDB *self, LogMessage *msg, const gchar *message, gssize message_len)
{
  PDBLookupParams lookup;

  pdb_lookup_state_init(&lookup, msg);
  lookup.message_handle = LM_V_NONE;
  lookup.message_string = message;
  lookup.message_len = message_len;
  return _pattern_db_process(self, &lookup, NULL);
}

void
pattern_db_debug_ruleset(PatternDB *self, LogMessage *msg, GArray *dbg_list)
{
  PDBLookupParams lookup;

  pdb_lookup_state_init(&lookup, msg);
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
  self->rate_limits = g_hash_table_new_full(correllation_key_hash, correllation_key_equal, NULL, (GDestroyNotify) pdb_rate_limit_free);
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
  class_handle = log_msg_get_value_handle(".classifier.class");
  rule_id_handle = log_msg_get_value_handle(".classifier.rule_id");
  context_id_handle = log_msg_get_value_handle(".classifier.context_id");
  system_tag = log_tags_get_by_name(".classifier.system");
  unknown_tag = log_tags_get_by_name(".classifier.unknown");
}
