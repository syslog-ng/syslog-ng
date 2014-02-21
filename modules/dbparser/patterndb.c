/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "logmsg.h"
#include "tags.h"
#include "template/templates.h"
#include "misc.h"
#include "filter/filter-expr-parser.h"
#include "logpipe.h"
#include "patterndb-int.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static NVHandle class_handle = 0;
static NVHandle rule_id_handle = 0;
static NVHandle context_id_handle = 0;
static LogTagId system_tag;
static LogTagId unknown_tag;

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

/*
 * NOTE: borrows "db" and consumes "key" contents.
 */
PDBContext *
pdb_context_new(PatternDB *db, PDBStateKey *key)
{
  PDBContext *self = g_new0(PDBContext, 1);

  self->messages = g_ptr_array_new();
  self->db = db;
  memcpy(&self->key, key, sizeof(self->key));

  if (self->key.pid)
    self->key.pid = g_strdup(self->key.pid);
  if (self->key.program)
    self->key.program = g_strdup(self->key.program);
  if (self->key.host)
    self->key.host = g_strdup(self->key.host);
  self->ref_cnt = 1;
  return self;
}

PDBContext *
pdb_context_ref(PDBContext *self)
{
  self->ref_cnt++;
  return self;
}

void
pdb_context_unref(PDBContext *self)
{
  gint i;

  if (--self->ref_cnt == 0)
    {
      for (i = 0; i < self->messages->len; i++)
        {
          log_msg_unref((LogMessage *) g_ptr_array_index(self->messages, i));
        }
      g_ptr_array_free(self->messages, TRUE);
      if (self->rule)
        pdb_rule_unref(self->rule);

      if (self->key.host)
        g_free((gchar *) self->key.host);
      if (self->key.program)
        g_free((gchar *) self->key.program);
      if (self->key.pid)
        g_free((gchar *) self->key.pid);
      g_free(self->key.session_id);
      g_free(self);
    }
}

/***************************************************************************
 * PDBRateLimit, represents a rate-limit state in the state hash table, is
 * marked with PSK_RATE_LIMIT in the hash table key
 ***************************************************************************/

PDBRateLimit *
pdb_rate_limit_new(PDBStateKey *key)
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

/*********************************************************
 * PDBStateKey, is the key in the state hash table
 *********************************************************/

static guint
pdb_state_key_hash(gconstpointer k)
{
  PDBStateKey *key = (PDBStateKey *) k;
  guint hash;

  hash = (key->scope << 30) + (key->type << 29);
  switch (key->scope)
    {
    case RCS_PROCESS:
      hash += g_str_hash(key->pid);
    case RCS_PROGRAM:
      hash += g_str_hash(key->program);
    case RCS_HOST:
      hash += g_str_hash(key->host);
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  return hash + g_str_hash(key->session_id);
}

static gboolean
pdb_state_key_equal(gconstpointer k1, gconstpointer k2)
{
  PDBStateKey *key1 = (PDBStateKey *) k1;
  PDBStateKey *key2 = (PDBStateKey *) k2;

  if (key1->scope != key2->scope || key1->type != key2->type)
    return FALSE;

  switch (key1->scope)
    {
    case RCS_PROCESS:
      if (strcmp(key1->pid, key2->pid) != 0)
        return FALSE;
    case RCS_PROGRAM:
      if (strcmp(key1->program, key2->program) != 0)
        return FALSE;
    case RCS_HOST:
      if (strcmp(key1->host, key2->host) != 0)
        return FALSE;
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  if (strcmp(key1->session_id, key2->session_id) != 0)
    return FALSE;
  return TRUE;
}

/* fills a PDBStateKey structure with borrowed values */
static void
pdb_state_key_setup(PDBStateKey *self, gint type, PDBRule *rule, LogMessage *msg, gchar *session_id)
{
  memset(self, 0, sizeof(*self));
  self->scope = rule->context_scope;
  self->type = type;
  self->session_id = session_id;

  /* NVTable ensures that builtin name-value pairs are always NUL terminated */
  switch (rule->context_scope)
    {
    case RCS_PROCESS:
      self->pid = log_msg_get_value(msg, LM_V_PID, NULL);
    case RCS_PROGRAM:
      self->program = log_msg_get_value(msg, LM_V_PROGRAM, NULL);
    case RCS_HOST:
      self->host = log_msg_get_value(msg, LM_V_HOST, NULL);
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

/*********************************************************
 * PDBStateEntry, is the value in the state hash table
 *********************************************************/

static void
pdb_state_entry_free(gpointer s)
{
  PDBStateEntry *self = (PDBStateEntry *) s;

  if (self->key.type == PSK_CONTEXT)
    pdb_context_unref(&self->context);
  else if (self->key.type == PSK_RATE_LIMIT)
    pdb_rate_limit_free(&self->rate_limit);
}

/*********************************************************
 * PDBMessage
 *********************************************************/

void
pdb_message_add_tag(PDBMessage *self, const gchar *text)
{
  LogTagId tag;

  if (!self->tags)
    self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));
  tag = log_tags_get_by_name(text);
  g_array_append_val(self->tags, tag);
}

static void
pdb_message_apply(PDBMessage *self, PDBContext *context, LogMessage *msg, GString *buffer)
{
  gint i;

  if (self->tags)
    {
      for (i = 0; i < self->tags->len; i++)
        log_msg_set_tag_by_id(msg, g_array_index(self->tags, LogTagId, i));
    }

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        {
          log_template_format_with_context(g_ptr_array_index(self->values, i),
                                           context ? (LogMessage **) context->messages->pdata : &msg,
                                           context ? context->messages->len : 1,
                                           NULL, LTZ_LOCAL, 0, context ? context->key.session_id : NULL, buffer);
          log_msg_set_value(msg,
                            log_msg_get_value_handle(((LogTemplate *) g_ptr_array_index(self->values, i))->name),
                            buffer->str, buffer->len);
        }
    }

}

void
pdb_message_clean(PDBMessage *self)
{
  gint i;

  if (self->tags)
    g_array_free(self->tags, TRUE);

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        log_template_unref(g_ptr_array_index(self->values, i));

      g_ptr_array_free(self->values, TRUE);
    }
}

void
pdb_message_free(PDBMessage *self)
{
  pdb_message_clean(self);
  g_free(self);
}

/*********************************************************
 * PDBAction
 *********************************************************/

void
pdb_action_set_condition(PDBAction *self, GlobalConfig *cfg, const gchar *filter_string, GError **error)
{
  CfgLexer *lexer;

  lexer = cfg_lexer_new_buffer(filter_string, strlen(filter_string));
  if (!cfg_run_parser(cfg, lexer, &filter_expr_parser, (gpointer *) &self->condition, NULL))
    {
      g_set_error(error, 0, 1, "Error compiling conditional expression");
      self->condition = NULL;
      return;
    }
}

void
pdb_action_set_rate(PDBAction *self, const gchar *rate_)
{
  gchar *slash;
  gchar *rate;

  rate = g_strdup(rate_);

  slash = strchr(rate, '/');
  if (!slash)
    {
      self->rate = atoi(rate);
      self->rate_quantum = 1;
    }
  else
    {
      *slash = 0;
      self->rate = atoi(rate);
      self->rate_quantum = atoi(slash + 1);
      *slash = '/';
    }
  if (self->rate_quantum == 0)
    self->rate_quantum = 1;
  g_free(rate);
}

void
pdb_action_set_trigger(PDBAction *self, const gchar *trigger, GError **error)
{
  if (strcmp(trigger, "match") == 0)
    self->trigger = RAT_MATCH;
  else if (strcmp(trigger, "timeout") == 0)
    self->trigger = RAT_TIMEOUT;
  else
    g_set_error(error, 0, 1, "Unknown trigger type: %s", trigger);
}

void
pdb_action_set_inheritance(PDBAction *self, const gchar *inherit_properties, GError **error)
{
  if (inherit_properties[0] == 'T' || inherit_properties[0] == 't' ||
      inherit_properties[0] == '1')
    self->inherit_properties = TRUE;
  else if (inherit_properties[0] == 'F' || inherit_properties[0] == 'f' ||
           inherit_properties[0] == '0')
    self->inherit_properties = FALSE;
  else
    g_set_error(error, 0, 1, "Unknown inheritance type: %s", inherit_properties);
}

PDBAction *
pdb_action_new(gint id)
{
  PDBAction *self;

  self = g_new0(PDBAction, 1);

  self->trigger = RAT_MATCH;
  self->content_type = RAC_NONE;
  self->id = id;
  return self;
}

void
pdb_action_free(PDBAction *self)
{
  if (self->condition)
    filter_expr_unref(self->condition);
  if (self->content_type == RAC_MESSAGE)
    pdb_message_clean(&self->content.message);
  g_free(self);
}

/*********************************************************
 * PDBRule
 *********************************************************/


void
pdb_rule_set_class(PDBRule *self, const gchar *class)
{
  gchar class_tag_text[32];

  if (self->class)
    {
      g_free(self->class);
    }
  else
    {
      g_snprintf(class_tag_text, sizeof(class_tag_text), ".classifier.%s", class);
      pdb_message_add_tag(&self->msg, class_tag_text);
    }
  self->class = class ? g_strdup(class) : NULL;

}

void
pdb_rule_set_rule_id(PDBRule *self, const gchar *rule_id)
{
  if (self->rule_id)
    g_free(self->rule_id);
  self->rule_id = g_strdup(rule_id);
}

void
pdb_rule_set_context_id_template(PDBRule *self, LogTemplate *context_id_template)
{
  if (self->context_id_template)
    log_template_unref(self->context_id_template);
  self->context_id_template = context_id_template;
}

void
pdb_rule_set_context_timeout(PDBRule *self, gint timeout)
{
  self->context_timeout = timeout;
}

void
pdb_rule_set_context_scope(PDBRule *self, const gchar *scope, GError **error)
{
  if (strcmp(scope, "global") ==  0)
    self->context_scope = RCS_GLOBAL;
  else if (strcmp(scope, "host") == 0)
    self->context_scope = RCS_HOST;
  else if (strcmp(scope, "program") == 0)
    self->context_scope = RCS_PROGRAM;
  else if (strcmp(scope, "process") == 0)
    self->context_scope = RCS_PROCESS;
  else
    g_set_error(error, 0, 1, "Unknown context scope: %s", scope);
}

void
pdb_rule_add_action(PDBRule *self, PDBAction *action)
{
  if (!self->actions)
    self->actions = g_ptr_array_new();
  g_ptr_array_add(self->actions, action);
}

static inline gboolean
pdb_rule_check_rate_limit(PDBRule *self, PatternDB *db, PDBAction *action, LogMessage *msg, GString *buffer)
{
  PDBStateKey key;
  PDBRateLimit *rl;
  guint64 now;

  if (action->rate == 0)
    return TRUE;

  g_string_printf(buffer, "%s:%d", self->rule_id, action->id);
  pdb_state_key_setup(&key, PSK_RATE_LIMIT, self, msg, buffer->str);

  rl = g_hash_table_lookup(db->state, &key);
  if (!rl)
    {
      rl = pdb_rate_limit_new(&key);
      g_hash_table_insert(db->state, &rl->key, rl);
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

void
pdb_rule_run_actions(PDBRule *self, gint trigger, PatternDB *db, PDBContext *context, LogMessage *msg, PatternDBEmitFunc emit, gpointer emit_data, GString *buffer)
{
  gint i;

  if (!self->actions)
    return;
  for (i = 0; i < self->actions->len; i++)
    {
      PDBAction *action = (PDBAction *) g_ptr_array_index(self->actions, i);

      if (action->trigger == trigger)
        {
          LogMessage *genmsg;

          if ((!action->condition ||
               (!context || filter_expr_eval_with_context(action->condition, (LogMessage **) context->messages->pdata, context->messages->len))) &&
              pdb_rule_check_rate_limit(self, db, action, msg, buffer))
            {
              switch (action->content_type)
                {
                case RAC_NONE:
                  break;
                case RAC_MESSAGE:
                  if (action->inherit_properties)
                    {
                      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
                      path_options.ack_needed = FALSE;
                      genmsg = log_msg_clone_cow(msg, &path_options);
                    }
                  else
                    {
                      genmsg = log_msg_new_empty();
                      genmsg->flags |= LF_LOCAL;
                      genmsg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_STAMP];
                    }
                  if (context)
                    {
                      switch (context->key.scope)
                        {
                          case RCS_PROCESS:
                            log_msg_set_value(genmsg, LM_V_PID, context->key.pid, -1);
                          case RCS_PROGRAM:
                            log_msg_set_value(genmsg, LM_V_PROGRAM, context->key.program, -1);
                          case RCS_HOST:
                            log_msg_set_value(genmsg, LM_V_HOST, context->key.host, -1);
                          case RCS_GLOBAL:
                            break;
                          default:
                            g_assert_not_reached();
                          break;
                        }
                    }
                  if (context)
                    {
                      g_ptr_array_add(context->messages, genmsg);
                      pdb_message_apply(&action->content.message, context, genmsg, buffer);
                      g_ptr_array_remove_index_fast(context->messages, context->messages->len - 1);
                    }
                  else
                    {
                      /* no context, which means no correllation. The action
                       * rule contains the generated message at @0 and the one
                       * which triggered the rule in @1.
                       *
                       * We emulate a context having only these two
                       * messages, but without allocating a full-blown
                       * structure.
                       */
                      LogMessage *dummy_msgs[] = { msg, genmsg, NULL };
                      GPtrArray dummy_ptr_array = { .pdata = (void **) dummy_msgs, .len = 2 };
                      PDBContext dummy_context = { .messages = &dummy_ptr_array, 0 };

                      pdb_message_apply(&action->content.message, &dummy_context, genmsg, buffer);
                    }

                  emit(genmsg, TRUE, emit_data);
                  log_msg_unref(genmsg);
                  break;
                default:
                  g_assert_not_reached();
                  break;
                }
            }
        }
    }
}

static PDBRule *
pdb_rule_new(void)
{
  PDBRule *self = g_new0(PDBRule, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->context_scope = RCS_PROCESS;
  return self;
}

static PDBRule *
pdb_rule_ref(PDBRule *self)
{
  g_atomic_counter_inc(&self->ref_cnt);
  return self;
}

void
pdb_rule_unref(PDBRule *self)
{
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      if (self->context_id_template)
        log_template_unref(self->context_id_template);

      if (self->actions)
        {
          g_ptr_array_foreach(self->actions, (GFunc) pdb_action_free, NULL);
          g_ptr_array_free(self->actions, TRUE);
        }

      if (self->rule_id)
        g_free(self->rule_id);

      if (self->class)
        g_free(self->class);

      pdb_message_clean(&self->msg);
      g_free(self);
    }
}

/*********************************************************
 * PDBExample
 *********************************************************/


void
pdb_example_free(PDBExample *self)
{
  gint i;

  if (self->rule)
    pdb_rule_unref(self->rule);

  if (self->message)
    g_free(self->message);

  if (self->program)
    g_free(self->program);

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        {
          gchar **nv = g_ptr_array_index(self->values, i);

          g_free(nv[0]);
          g_free(nv[1]);
          g_free(nv);
        }

      g_ptr_array_free(self->values, TRUE);
    }

  g_free(self);
}

static gchar *
pdb_rule_get_name(gpointer s)
{
  PDBRule *self = (PDBRule *) s;

  if (self)
    return self->rule_id;
  else
    return NULL;
}

/*********************************************************
 * PDBProgram
 *********************************************************/

/*
 * Database based parser. The patterns are stored in an XML database.
 * Data structure is:
 *   - Parser -> programs -> rules -> patterns
 */

PDBProgram *
pdb_program_new(void)
{
  PDBProgram *self = g_new0(PDBProgram, 1);

  self->rules = r_new_node("", NULL);
  self->ref_cnt = 1;
  return self;
}

static PDBProgram *
pdb_program_ref(PDBProgram *self)
{
  self->ref_cnt++;
  return self;
}

static void
pdb_program_unref(PDBProgram *s)
{
  PDBProgram *self = (PDBProgram *) s;

  if (--self->ref_cnt == 0)
    {
      if (self->rules)
        r_free_node(self->rules, (void (*)(void *)) pdb_rule_unref);

      g_free(self);
    }
}

/*********************************************************
 * PDBRuleSet
 *********************************************************/

/* arguments passed to the markup parser functions */
typedef struct _PDBLoader
{
  PDBRuleSet *ruleset;
  PDBProgram *root_program;
  PDBProgram *current_program;
  PDBRule *current_rule;
  PDBAction *current_action;
  PDBExample *current_example;
  PDBMessage *current_message;
  gboolean first_program;
  gboolean in_pattern;
  gboolean in_ruleset;
  gboolean in_rule;
  gboolean in_tag;
  gboolean in_example;
  gboolean in_test_msg;
  gboolean in_test_value;
  gboolean in_action;
  gboolean load_examples;
  GList *examples;
  gchar *value_name;
  gchar *test_value_name;
  GlobalConfig *cfg;
  gint action_id;
  GArray *program_patterns;
} PDBLoader;

typedef struct _PDBProgramPattern
{
  gchar *pattern;
  PDBRule *rule;
} PDBProgramPattern;

void
pdb_loader_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names,
                         const gchar **attribute_values, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;
  gint i;

  if (strcmp(element_name, "ruleset") == 0)
    {
      if (state->in_ruleset)
        {
          *error = g_error_new(1, 1, "Unexpected <ruleset> element");
          return;
        }

      state->in_ruleset = TRUE;
      state->first_program = TRUE;
      state->program_patterns = g_array_new(0, 0, sizeof(PDBProgramPattern));
    }
  else if (strcmp(element_name, "example") == 0)
    {
      if (state->in_example || !state->in_rule)
        {
          *error = g_error_new(1, 1, "Unexpected <example> element");
          return;
        }

      state->in_example = TRUE;
      state->current_example = g_new0(PDBExample, 1);
      state->current_example->rule = pdb_rule_ref(state->current_rule);
    }
  else if (strcmp(element_name, "test_message") == 0)
    {
      if (state->in_test_msg || !state->in_example)
        {
          *error = g_error_new(1, 1, "Unexpected <test_message> element");
          return;
        }

      state->in_test_msg = TRUE;

      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "program") == 0)
            state->current_example->program = g_strdup(attribute_values[i]);
        }
    }
  else if (strcmp(element_name, "test_value") == 0)
    {
      if (state->in_test_value || !state->in_example)
        {
          *error = g_error_new(1, 1, "Unexpected <test_value> element");
          return;
        }

      state->in_test_value = TRUE;

      if (attribute_names[0] && g_str_equal(attribute_names[0], "name"))
        state->test_value_name = g_strdup(attribute_values[0]);
      else
        {
          msg_error("No name is specified for test_value",
                    evt_tag_str("rule_id", state->current_rule->rule_id),
                    NULL);
          *error = g_error_new(1, 0, "<test_value> misses name attribute");
          return;
        }
    }
  else if (strcmp(element_name, "rule") == 0)
    {
      if (state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <rule> element");
          return;
        }

      state->current_rule = pdb_rule_new();
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "class") == 0)
            pdb_rule_set_class(state->current_rule, attribute_values[i]);
          else if (strcmp(attribute_names[i], "id") == 0)
            pdb_rule_set_rule_id(state->current_rule, attribute_values[i]);
          else if (strcmp(attribute_names[i], "context-id") == 0)
            {
              LogTemplate *template;

              template = log_template_new(state->cfg, NULL);
              log_template_compile(template, attribute_values[i], NULL);
              pdb_rule_set_context_id_template(state->current_rule, template);
            }
          else if (strcmp(attribute_names[i], "context-timeout") == 0)
            pdb_rule_set_context_timeout(state->current_rule, strtol(attribute_values[i], NULL, 0));
          else if (strcmp(attribute_names[i], "context-scope") == 0)
            pdb_rule_set_context_scope(state->current_rule, attribute_values[i], error);
        }

      if (!state->current_rule->rule_id)
        {
          *error = g_error_new(1, 0, "No id attribute for rule element");
          pdb_rule_unref(state->current_rule);
          state->current_rule = NULL;
          return;
        }

      state->in_rule = TRUE;
      state->current_message = &state->current_rule->msg;
      state->action_id = 0;
    }
  else if (strcmp(element_name, "pattern") == 0)
    {
      state->in_pattern = TRUE;
    }
  else if (strcmp(element_name, "tag") == 0)
    {
      state->in_tag = TRUE;
    }
  else if (strcmp(element_name, "value") == 0)
    {
      if (attribute_names[0] && g_str_equal(attribute_names[0], "name"))
        state->value_name = g_strdup(attribute_values[0]);
      else
        {
          msg_error("No name is specified for value", evt_tag_str("rule_id", state->current_rule->rule_id), NULL);
          *error = g_error_new(1, 0, "<value> misses name attribute");
          return;
        }
    }
  else if (strcmp(element_name, "patterndb") == 0)
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "version") == 0)
            state->ruleset->version = g_strdup(attribute_values[i]);
          else if (strcmp(attribute_names[i], "pub_date") == 0)
            state->ruleset->pub_date = g_strdup(attribute_values[i]);
        }
      if (!state->ruleset->version)
        {
          msg_warning("patterndb version is unspecified, assuming v4 format", NULL);
          state->ruleset->version = g_strdup("4");
        }
      else if (state->ruleset->version && atoi(state->ruleset->version) < 2)
        {
          *error = g_error_new(1, 0, "patterndb version too old, this version of syslog-ng only supports v3 and v4 formatted patterndb files, please upgrade it using pdbtool");
          return;
        }
      else if (state->ruleset->version && atoi(state->ruleset->version) > 4)
        {
          *error = g_error_new(1, 0, "patterndb version too new, this version of syslog-ng supports v3 and v4 formatted patterndb files.");
          return;
        }
    }
  else if (strcmp(element_name, "action") == 0)
    {
      if (!state->current_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <action> element, it must be inside a rule");
          return;
        }
      state->current_action = pdb_action_new(state->action_id++);

      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "trigger") == 0)
            pdb_action_set_trigger(state->current_action, attribute_values[i], error);
          else if (strcmp(attribute_names[i], "condition") == 0)
            pdb_action_set_condition(state->current_action, state->cfg, attribute_values[i], error);
          else if (strcmp(attribute_names[i], "rate") == 0)
            pdb_action_set_rate(state->current_action, attribute_values[i]);
        }
      state->in_action = TRUE;
    }
  else if (strcmp(element_name, "message") == 0)
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "inherit-properties") == 0)
            pdb_action_set_inheritance(state->current_action, attribute_values[i], error);
        }
      if (!state->in_action)
        {
          *error = g_error_new(1, 0, "Unexpected <message> element, it must be inside an action");
          return;
        }
      state->current_action->content_type = RAC_MESSAGE;
      state->current_message = &state->current_action->content.message;
    }
}

void
pdb_loader_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;
  PDBProgramPattern *program_pattern;
  PDBProgram *program;
  int i;

  if (strcmp(element_name, "ruleset") == 0)
    {
      if (!state->in_ruleset)
        {
          *error = g_error_new(1, 0, "Unexpected </ruleset> element");
          return;
        }

      program = (state->current_program ? state->current_program : state->root_program);

      /* Copy stored rules into current program */
      for (i = 0; i < state->program_patterns->len; i++)
        {
          program_pattern = &g_array_index(state->program_patterns, PDBProgramPattern, i);

          r_insert_node(program->rules,
                        program_pattern->pattern,
                        program_pattern->rule,
                        TRUE, pdb_rule_get_name);
          g_free(program_pattern->pattern);
        }

      state->current_program = NULL;
      state->in_ruleset = FALSE;

      g_array_free(state->program_patterns, TRUE);
      state->program_patterns = NULL;
    }
  else if (strcmp(element_name, "example") == 0)
    {
      if (!state->in_example)
        {
          *error = g_error_new(1, 0, "Unexpected </example> element");
          return;
        }

      state->in_example = FALSE;

      if (state->load_examples)
        state->examples = g_list_prepend(state->examples, state->current_example);
      else
        pdb_example_free(state->current_example);

      state->current_example = NULL;
    }
  else if (strcmp(element_name, "test_message") == 0)
    {
      if (!state->in_test_msg)
        {
          *error = g_error_new(1, 0, "Unexpected </test_message> element");
          return;
        }

      state->in_test_msg = FALSE;
    }
  else if (strcmp(element_name, "test_value") == 0)
    {
      if (!state->in_test_value)
        {
          *error = g_error_new(1, 0, "Unexpected </test_value> element");
          return;
        }

      state->in_test_value = FALSE;

      if (state->test_value_name)
        g_free(state->test_value_name);

      state->test_value_name = NULL;
    }
  else if (strcmp(element_name, "rule") == 0)
    {
      if (!state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected </rule> element");
          return;
        }

      state->in_rule = FALSE;
      if (state->current_rule)
        {
          pdb_rule_unref(state->current_rule);
          state->current_rule = NULL;
        }
      state->current_message = NULL;
    }
  else if (strcmp(element_name, "value") == 0)
    {
      if (state->value_name)
        g_free(state->value_name);

      state->value_name = NULL;
    }
  else if (strcmp(element_name, "pattern") == 0)
    state->in_pattern = FALSE;
  else if (strcmp(element_name, "tag") == 0)
    state->in_tag = FALSE;
  else if (strcmp(element_name, "action") == 0)
    {
      state->in_action = FALSE;
      pdb_rule_add_action(state->current_rule, state->current_action);
      state->current_action = NULL;
    }
  else if (strcmp(element_name, "message") == 0)
    {
      state->current_message = &state->current_rule->msg;
    }
}

void
pdb_loader_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;
  LogTemplate *value;
  GError *err = NULL;
  PDBProgramPattern program_pattern;
  RNode *node = NULL;
  gchar *txt;
  gchar **nv;

  if (state->in_pattern)
    {
      txt = g_strdup(text);

      if (state->in_rule)
        {
          program_pattern.pattern = g_strdup(text);
          program_pattern.rule = pdb_rule_ref(state->current_rule);
          g_array_append_val(state->program_patterns, program_pattern);
        }
      else if (state->in_ruleset)
        {

          if (state->first_program)
            {
              node = r_find_node(state->ruleset->programs, txt, txt, strlen(txt), NULL);

              if (node && node->value && node != state->ruleset->programs)
                state->current_program = node->value;
              else
                {
                  state->current_program = pdb_program_new();

                  r_insert_node(state->ruleset->programs,
                            txt,
                            state->current_program,
                            TRUE, NULL);
                }
              state->first_program = FALSE;
            }
          else if (state->current_program)
            {
              node = r_find_node(state->ruleset->programs, txt, txt, strlen(txt), NULL);

              if (!node || !node->value || node == state->ruleset->programs)
                {
                  r_insert_node(state->ruleset->programs,
                            txt,
                            pdb_program_ref(state->current_program),
                            TRUE, NULL);
                }
            }
        }
      g_free(txt);
    }
  else if (state->in_tag)
    {
      if (!state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <tag> element, must be within a rule");
          return;
        }
      pdb_message_add_tag(state->current_message, text);
    }
  else if (state->value_name)
    {
      if (!state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <value> element, must be within a rule");
          return;
        }
      if (!state->current_message->values)
        state->current_message->values = g_ptr_array_new();

      value = log_template_new(state->cfg, state->value_name);
      if (!log_template_compile(value, text, &err))
        {
          msg_error("Error compiling value template",
            evt_tag_str("name", state->value_name),
            evt_tag_str("value", text),
            evt_tag_str("error", err->message), NULL);
          log_template_unref(value);
          g_propagate_error(error, err);
          return;
        }
      else
        g_ptr_array_add(state->current_message->values, value);
    }
  else if (state->in_test_msg)
    {
      state->current_example->message = g_strdup(text);
    }
  else if (state->in_test_value)
    {
      if (!state->current_example->values)
        state->current_example->values = g_ptr_array_new();

      nv = g_new(gchar *, 2);
      nv[0] = state->test_value_name;
      state->test_value_name = NULL;
      nv[1] = g_strdup(text);

      g_ptr_array_add(state->current_example->values, nv);
    }
}

static
GMarkupParser db_parser =
{
  .start_element = pdb_loader_start_element,
  .end_element = pdb_loader_end_element,
  .text = pdb_loader_text,
  .passthrough = NULL,
  .error = NULL
};

gboolean
pdb_rule_set_load(PDBRuleSet *self, GlobalConfig *cfg, const gchar *config, GList **examples)
{
  PDBLoader state;
  GMarkupParseContext *parse_ctx = NULL;
  GError *error = NULL;
  FILE *dbfile = NULL;
  gint bytes_read;
  gchar buff[4096];
  gboolean success = FALSE;

  if ((dbfile = fopen(config, "r")) == NULL)
    {
      msg_error("Error opening classifier configuration file",
                 evt_tag_str(EVT_TAG_FILENAME, config),
                 evt_tag_errno(EVT_TAG_OSERROR, errno),
                 NULL);
      goto error;
    }

  memset(&state, 0x0, sizeof(state));

  state.ruleset = self;
  state.root_program = pdb_program_new();
  state.load_examples = !!examples;
  state.cfg = cfg;

  self->programs = r_new_node("", state.root_program);

  parse_ctx = g_markup_parse_context_new(&db_parser, 0, &state, NULL);

  while ((bytes_read = fread(buff, sizeof(gchar), 4096, dbfile)) != 0)
    {
      if (!g_markup_parse_context_parse(parse_ctx, buff, bytes_read, &error))
        {
          msg_error("Error parsing pattern database file",
                    evt_tag_str(EVT_TAG_FILENAME, config),
                    evt_tag_str("error", error ? error->message : "unknown"),
                    NULL);
          goto error;
        }
    }
  fclose(dbfile);
  dbfile = NULL;

  if (!g_markup_parse_context_end_parse(parse_ctx, &error))
    {
      msg_error("Error parsing pattern database file",
                evt_tag_str(EVT_TAG_FILENAME, config),
                evt_tag_str("error", error ? error->message : "unknown"),
                NULL);
      goto error;
    }

  if (state.load_examples)
    *examples = state.examples;

  success = TRUE;

 error:
  if (dbfile)
    fclose(dbfile);
  if (parse_ctx)
    g_markup_parse_context_free(parse_ctx);
  return success;
}

/**
 * log_db_add_matches:
 *
 * Adds the values from the given GArray of RParserMatch entries to the NVTable
 * of the passed LogMessage.
 *
 * @msg: the LogMessage to add the matches to
 * @matches: an array of RParserMatch entries
 * @ref_handle: if the matches are indirect matches, they are referenced based on this handle (eg. LM_V_MESSAGE)
 **/
void
log_db_add_matches(LogMessage *msg, GArray *matches, NVHandle ref_handle, const gchar *input_string)
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
pdb_rule_set_lookup(PDBRuleSet *self, PDBInput *input, GArray *dbg_list)
{
  RNode *node;
  LogMessage *msg = input->msg;
  GArray *prg_matches, *matches;
  const gchar *program;
  gssize program_len;

  if (G_UNLIKELY(!self->programs))
    return FALSE;

  program = log_msg_get_value(msg, input->program_handle, &program_len);
  prg_matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
  node = r_find_node(self->programs, (gchar *) program, (gchar *) program, program_len, prg_matches);

  if (node)
    {
      log_db_add_matches(msg, prg_matches, input->program_handle, program);
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

          if (input->message_handle)
            {
              message = log_msg_get_value(msg, input->message_handle, &message_len);
            }
          else
            {
              message = input->message_string;
              message_len = input->message_len;
            }

          if (G_UNLIKELY(dbg_list))
            msg_node = r_find_node_dbg(program->rules, (gchar *) message, (gchar *) message, message_len, matches, dbg_list);
          else
            msg_node = r_find_node(program->rules, (gchar *) message, (gchar *) message, message_len, matches);

          if (msg_node)
            {
              PDBRule *rule = (PDBRule *) msg_node->value;
              GString *buffer = g_string_sized_new(32);

              msg_debug("patterndb rule matches",
                        evt_tag_str("rule_id", rule->rule_id),
                        NULL);
              log_msg_set_value(msg, class_handle, rule->class ? rule->class : "system", -1);
              log_msg_set_value(msg, rule_id_handle, rule->rule_id, -1);

              log_db_add_matches(msg, matches, input->message_handle, message);
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

PDBRuleSet *
pdb_rule_set_new(void)
{
  PDBRuleSet *self = g_new0(PDBRuleSet, 1);

  return self;
}

void
pdb_rule_set_free(PDBRuleSet *self)
{
  if (self->programs)
    r_free_node(self->programs, (GDestroyNotify) pdb_program_unref);
  if (self->version)
    g_free(self->version);
  if (self->pub_date)
    g_free(self->pub_date);
  self->programs = NULL;
  self->version = NULL;
  self->pub_date = NULL;

  g_free(self);
}

/*********************************************************
 * PatternDB
 *********************************************************/

static void
pattern_db_expire_entry(guint64 now, gpointer user_data)
{
  PDBContext *context = user_data;
  PatternDB *pdb = context->db;
  GString *buffer = g_string_sized_new(256);

  msg_debug("Expiring patterndb correllation context",
            evt_tag_str("last_rule", context->rule->rule_id),
            evt_tag_long("utc", timer_wheel_get_time(context->db->timer_wheel)),
            NULL);
  if (pdb->emit)
    pdb_rule_run_actions(context->rule, RAT_TIMEOUT, context->db, context, g_ptr_array_index(context->messages, context->messages->len - 1), pdb->emit, pdb->emit_data, buffer);
  g_hash_table_remove(context->db->state, &context->key);
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
                evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
                NULL);
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
            evt_tag_long("utc", timer_wheel_get_time(self->timer_wheel)),
            NULL);
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
pattern_db_expire_state(PatternDB *self)
{
  g_static_rw_lock_writer_lock(&self->lock);
  timer_wheel_expire_all(self->timer_wheel);
  g_static_rw_lock_writer_unlock(&self->lock);
}

void
pattern_db_forget_state(PatternDB *self)
{
  g_static_rw_lock_writer_lock(&self->lock);
  if (self->timer_wheel)
    timer_wheel_free(self->timer_wheel);

  if (self->state)
    g_hash_table_destroy(self->state);
  self->state = g_hash_table_new_full(pdb_state_key_hash, pdb_state_key_equal, NULL, (GDestroyNotify) pdb_state_entry_free);
  self->timer_wheel = timer_wheel_new();
  g_static_rw_lock_writer_unlock(&self->lock);
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

gboolean
pattern_db_process(PatternDB *self, PDBInput *input)
{
  PDBRule *rule;
  LogMessage *msg = input->msg;

  if (G_UNLIKELY(!self->ruleset))
    return FALSE;

  g_static_rw_lock_reader_lock(&self->lock);
  rule = pdb_rule_set_lookup(self->ruleset, input, NULL);
  g_static_rw_lock_reader_unlock(&self->lock);
  if (rule)
    {
      PDBContext *context = NULL;
      GString *buffer = g_string_sized_new(32);

      g_static_rw_lock_writer_lock(&self->lock);
      pattern_db_set_time(self, &msg->timestamps[LM_TS_STAMP]);
      if (rule->context_id_template)
        {
          PDBStateKey key;

          log_template_format(rule->context_id_template, msg, NULL, LTZ_LOCAL, 0, NULL, buffer);
          log_msg_set_value(msg, context_id_handle, buffer->str, -1);

          pdb_state_key_setup(&key, PSK_CONTEXT, rule, msg, buffer->str);
          context = g_hash_table_lookup(self->state, &key);
          if (!context)
            {
              msg_debug("Correllation context lookup failure, starting a new context",
                        evt_tag_str("rule", rule->rule_id),
                        evt_tag_str("context", buffer->str),
                        evt_tag_int("context_timeout", rule->context_timeout),
                        evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + rule->context_timeout),
                        NULL);
              context = pdb_context_new(self, &key);
              g_hash_table_insert(self->state, &context->key, context);
              g_string_steal(buffer);
            }
          else
            {
              msg_debug("Correllation context lookup successful",
                        evt_tag_str("rule", rule->rule_id),
                        evt_tag_str("context", buffer->str),
                        evt_tag_int("context_timeout", rule->context_timeout),
                        evt_tag_int("context_expiration", timer_wheel_get_time(self->timer_wheel) + rule->context_timeout),
                        evt_tag_int("num_messages", context->messages->len),
                        NULL);
            }

          g_ptr_array_add(context->messages, log_msg_ref(msg));

          if (context->timer)
            {
              timer_wheel_mod_timer(self->timer_wheel, context->timer, rule->context_timeout);
            }
          else
            {
              context->timer = timer_wheel_add_timer(self->timer_wheel, rule->context_timeout, pattern_db_expire_entry, pdb_context_ref(context), (GDestroyNotify) pdb_context_unref);
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

      pdb_message_apply(&rule->msg, context, msg, buffer);
      if (self->emit)
        {
          self->emit(msg, FALSE, self->emit_data);
          pdb_rule_run_actions(rule, RAT_MATCH, self, context, msg, self->emit, self->emit_data, buffer);
        }
      pdb_rule_unref(rule);
      g_static_rw_lock_writer_unlock(&self->lock);

      if (context)
        log_msg_write_protect(msg);

      g_string_free(buffer, TRUE);
    }
  else
    {
      g_static_rw_lock_writer_lock(&self->lock);
      pattern_db_set_time(self, &msg->timestamps[LM_TS_STAMP]);
      g_static_rw_lock_writer_unlock(&self->lock);
      if (self->emit)
        self->emit(msg, FALSE, self->emit_data);
    }
  return rule != NULL;
}


PatternDB *
pattern_db_new(void)
{
  PatternDB *self = g_new0(PatternDB, 1);

  self->ruleset = pdb_rule_set_new();
  self->state = g_hash_table_new_full(pdb_state_key_hash, pdb_state_key_equal, NULL, (GDestroyNotify) pdb_state_entry_free);
  self->timer_wheel = timer_wheel_new();
  cached_g_current_time(&self->last_tick);
  g_static_rw_lock_init(&self->lock);
  return self;
}

void
pattern_db_free(PatternDB *self)
{
  if (self->ruleset)
    pdb_rule_set_free(self->ruleset);

  if (self->state)
    g_hash_table_destroy(self->state);
  if (self->timer_wheel)
    timer_wheel_free(self->timer_wheel);
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
