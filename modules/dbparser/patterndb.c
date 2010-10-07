/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "patterndb.h"
#include "logmsg.h"
#include "tags.h"
#include "templates.h"
#include "compat.h"
#include "misc.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

/*********************************************************
 * PDBMessage
 *********************************************************/

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
                                           NULL, LTZ_LOCAL, 0, buffer);
          log_msg_set_value(msg,
                            log_msg_get_value_handle(((LogTemplate *) g_ptr_array_index(self->values, i))->name),
                            buffer->str, buffer->len);
        }
    }

}

void
pdb_message_init(PDBMessage *self)
{
  self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));
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
 * PDBRule
 *********************************************************/

void
pdb_rule_set_class(PDBRule *self, const gchar *class)
{
  LogTagId class_tag;
  gchar class_tag_text[32];

  if (self->class)
    {
      g_free(self->class);
    }
  else
    {
      /* FIXME: the class == NULL handling should be done at apply time */
      g_snprintf(class_tag_text, sizeof(class_tag_text), ".classifier.%s", class ? class : "system");
      class_tag = log_tags_get_by_name(class_tag_text);
      g_array_append_val(self->msg.tags, class_tag);
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
pdb_rule_set_context_scope(PDBRule *self, const gchar *scope)
{
  /* FIXME */
}

PDBRule *
pdb_rule_new(void)
{
  PDBRule *self = g_new0(PDBRule, 1);

  self->ref_cnt = 1;
  pdb_message_init(&self->msg);
  return self;
}

PDBRule *
pdb_rule_ref(PDBRule *self)
{
  g_assert(self->ref_cnt > 0);

  self->ref_cnt++;

  return self;
}

static void
pdb_rule_unref(void *s)
{
  PDBRule *self = (PDBRule *) s;

  g_assert(self->ref_cnt > 0);

  if (--(self->ref_cnt) == 0)
    {
      if (self->context_id_template)
        log_template_unref(self->context_id_template);

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
        r_free_node(self->rules, pdb_rule_unref);

      g_free(self);
    }
}

/*
 * NOTE: borrows "db" and consumes "id" parameters.
 */
PDBContext *
pdb_context_new(PatternDB *db, gchar *id)
{
  PDBContext *self = g_new0(PDBContext, 1);

  self->messages = g_ptr_array_new();
  self->db = db;
  self->id = id;
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
      g_free(self->id);
      g_free(self);
    }
}

typedef struct _PDBLoader
{
  PatternDB *db;
  PDBProgram *root_program;
  PDBProgram *current_program;
  PDBRule *current_rule;
  PDBExample *current_example;
  gboolean first_program;
  gboolean in_pattern;
  gboolean in_ruleset;
  gboolean in_rule;
  gboolean in_tag;
  gboolean in_example;
  gboolean in_test_msg;
  gboolean in_test_value;
  gboolean load_examples;
  GList *examples;
  gchar *value_name;
  gchar *test_value_name;
  GlobalConfig *cfg;
} PDBLoader;

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
        msg_error("No name is specified for test_value",
                  evt_tag_str("rule_id", state->current_rule->rule_id),
                  NULL);
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
            pdb_rule_set_context_id_template(state->current_rule, log_template_new(state->cfg, NULL, attribute_values[i]));
          else if (strcmp(attribute_names[i], "context-timeout") == 0)
            pdb_rule_set_context_timeout(state->current_rule, strtol(attribute_values[i], NULL, 0));
          else if (strcmp(attribute_names[i], "context-scope") == 0)
            pdb_rule_set_context_scope(state->current_rule, attribute_values[i]);
        }

      if (!state->current_rule->rule_id)
        {
          *error = g_error_new(1, 0, "No id attribute for rule element");
          pdb_rule_unref(state->current_rule);
          state->current_rule = NULL;
          return;
        }

      state->in_rule = TRUE;
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
        msg_error("No name is specified for value", evt_tag_str("rule_id", state->current_rule->rule_id), NULL);
    }
  else if (strcmp(element_name, "patterndb") == 0)
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "version") == 0)
            state->db->version = g_strdup(attribute_values[i]);
          else if (strcmp(attribute_names[i], "pub_date") == 0)
            state->db->pub_date = g_strdup(attribute_values[i]);
        }
    }
}

void
pdb_loader_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;

  if (strcmp(element_name, "ruleset") == 0)
    {
      if (!state->in_ruleset)
        {
          *error = g_error_new(1, 0, "Unexpected </ruleset> element");
          return;
        }

      state->current_program = NULL;
      state->in_ruleset = FALSE;
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
}

void
pdb_loader_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;
  LogTemplate *value;
  GError *err = NULL;
  RNode *node = NULL;
  gchar *txt;
  gchar **nv;
  LogTagId tag;

  if (state->in_pattern)
    {
      txt = g_strdup(text);

      if (state->in_rule)
        {
          r_insert_node(state->current_program ? state->current_program->rules : state->root_program->rules,
                        txt,
                        pdb_rule_ref(state->current_rule),
                        TRUE, pdb_rule_get_name);
        }
      else if (state->in_ruleset)
        {

          if (state->first_program)
            {
              node = r_find_node(state->db->programs, txt, txt, strlen(txt), NULL);

              if (node && node->value && node != state->db->programs)
                state->current_program = node->value;
              else
                {
                  state->current_program = pdb_program_new();

                  r_insert_node(state->db->programs,
                            txt,
                            state->current_program,
                            TRUE, NULL);
                }
              state->first_program = FALSE;
            }
          else if (state->current_program)
            {
              node = r_find_node(state->db->programs, txt, txt, strlen(txt), NULL);

              if (!node || !node->value || node == state->db->programs)
                {
                  r_insert_node(state->db->programs,
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
      tag = log_tags_get_by_name(text);
      g_array_append_val(state->current_rule->msg.tags, tag);
    }
  else if (state->value_name)
    {
      if (!state->current_rule->msg.values)
        state->current_rule->msg.values = g_ptr_array_new();

      value = log_template_new(state->cfg, state->value_name, text);
      if (!log_template_compile(value, &err))
        {
          msg_error("Error compiling value template",
            evt_tag_str("name", state->value_name),
            evt_tag_str("value", text),
            evt_tag_str("error", err->message), NULL);
          g_clear_error(&err);
          log_template_unref(value);
        }
      else
        g_ptr_array_add(state->current_rule->msg.values, value);
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

GMarkupParser db_parser =
{
  .start_element = pdb_loader_start_element,
  .end_element = pdb_loader_end_element,
  .text = pdb_loader_text,
  .passthrough = NULL,
  .error = NULL
};

static NVHandle class_handle = 0;
static NVHandle rule_id_handle = 0;

static void
pattern_db_expire_state(guint64 now, gpointer user_data)
{
  PDBContext *context = user_data;

  g_hash_table_remove(context->db->state, context->id);

  /* pdb_context_free is automatically called when returning from
     this function by the timerwheel code as a destroy notify
     callback. */
}

gboolean
pattern_db_lookup(PatternDB *self, LogMessage *msg, GSList **dbg_list)
{
  RNode *node;
  GArray *matches;
  const gchar *program;
  gssize program_len;

  if (G_UNLIKELY(!self->programs))
    return FALSE;

  timer_wheel_set_time(self->timer_wheel, msg->timestamps[LM_TS_STAMP].time.tv_sec);
  program = log_msg_get_value(msg, LM_V_PROGRAM, &program_len);

  node = r_find_node(self->programs, (gchar *) program, (gchar *) program, program_len, NULL);

  if (node)
    {
      PDBProgram *program = (PDBProgram *) node->value;

      if (program->rules)
        {
          RNode *msg_node;
          const gchar *message;
          gssize message_len;
          PDBContext *context = NULL;

          /* NOTE: We're not using g_array_sized_new as that does not
           * correctly zero-initialize the new items even if clear_ is TRUE
           */

          matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
          g_array_set_size(matches, 1);

          message = log_msg_get_value(msg, LM_V_MESSAGE, &message_len);
          if (G_UNLIKELY(dbg_list))
            msg_node = r_find_node_dbg(program->rules, (gchar *) message, (gchar *) message, message_len, matches, dbg_list);
          else
            msg_node = r_find_node(program->rules, (gchar *) message, (gchar *) message, message_len, matches);

          if (msg_node)
            {
              PDBRule *rule = (PDBRule *) msg_node->value;
              GString *buffer = g_string_sized_new(32);
              gint i;

              log_msg_set_value(msg, class_handle, rule->class ? rule->class : "system", -1);
              log_msg_set_value(msg, rule_id_handle, rule->rule_id, -1);

              for (i = 0; i < matches->len; i++)
                {
                  RParserMatch *match = &g_array_index(matches, RParserMatch, i);

                  if (match->match)
                    {
                      log_msg_set_value(msg, match->handle, match->match, match->len);
                      g_free(match->match);
                    }
                  else
                    {
                      log_msg_set_value_indirect(msg, match->handle, LM_V_MESSAGE, match->type, match->ofs, match->len);
                    }
                }

              g_array_free(matches, TRUE);

              if (rule->context_id_template)
                {
                  log_template_format(rule->context_id_template, msg, NULL, LTZ_LOCAL, 0, buffer);
                  context = g_hash_table_lookup(self->state, buffer->str);
                  if (!context)
                    {
                      context = pdb_context_new(self, buffer->str);
                      g_hash_table_insert(self->state, context->id, context);
                      g_string_steal(buffer);
                    }

                  g_ptr_array_add(context->messages, log_msg_ref(msg));

                  if (context->timer)
                    {
                      timer_wheel_mod_timer(self->timer_wheel, context->timer, rule->context_timeout);
                    }
                  else
                    {
                      context->timer = timer_wheel_add_timer(self->timer_wheel, rule->context_timeout, pattern_db_expire_state, pdb_context_ref(context), (GDestroyNotify) pdb_context_unref);
                    }
                }
              else
                {
                  context = NULL;
                }

              pdb_message_apply(&rule->msg, context, msg, buffer);
              g_string_free(buffer, TRUE);
              return TRUE;
            }
          else
            {
              log_msg_set_value(msg, class_handle, "unknown", 7);
            }
          g_array_free(matches, TRUE);
        }
    }
  return FALSE;
}

gboolean
pattern_db_load(PatternDB *self, GlobalConfig *cfg, const gchar *config, GList **examples)
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

  state.db = self;
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
  if (!success)
    {
      pattern_db_free(self);
    }
  if (dbfile)
    fclose(dbfile);
  if (parse_ctx)
    g_markup_parse_context_free(parse_ctx);
  return success;
}

PatternDB *
pattern_db_new(void)
{
  PatternDB *self = g_new0(PatternDB, 1);

  self->state = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) pdb_context_unref);
  self->timer_wheel = timer_wheel_new();
  return self;
}

void
pattern_db_free(PatternDB *self)
{
  if (self->state)
    g_hash_table_destroy(self->state);
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

void
pattern_db_global_init(void)
{
  class_handle = log_msg_get_value_handle(".classifier.class");
  rule_id_handle = log_msg_get_value_handle(".classifier.rule_id");
}
