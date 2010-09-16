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

#include "logpatterns.h"
#include "logmsg.h"
#include "tags.h"
#include "templates.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>


LogDBResult *
log_db_result_new(gchar *class, gchar *rule_id)
{
  LogDBResult *self = g_new0(LogDBResult, 1);
  LogTagId class_tag;
  gchar class_tag_text[32];

  self->class = class;
  self->rule_id = rule_id;
  self->ref_cnt = 1;
  self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));

  g_snprintf(class_tag_text, sizeof(class_tag_text), ".classifier.%s", class ? class : "system");
  class_tag = log_tags_get_by_name(class_tag_text);
  g_array_append_val(self->tags, class_tag);

  return self;
}

LogDBResult *
log_db_result_ref(LogDBResult *self)
{
  g_assert(self->ref_cnt > 0);

  self->ref_cnt++;

  return self;
}

static void
log_db_result_unref(void *s)
{
  LogDBResult *self = (LogDBResult *) s;
  gint i;

  g_assert(self->ref_cnt > 0);

  if (--(self->ref_cnt) == 0)
    {
      if (self->rule_id)
        g_free(self->rule_id);

      if (self->class)
        g_free(self->class);

      if (self->tags)
        g_array_free(self->tags, TRUE);

      if (self->values)
        {
          for (i = 0; i < self->values->len; i++)
            log_template_unref(g_ptr_array_index(self->values, i));

          g_ptr_array_free(self->values, TRUE);
        }

      g_free(self);
    }
}

void
log_pattern_example_free(gpointer s)
{
  LogDBExample *self = (LogDBExample *) s;
  gint i;

  if (self->result)
    log_db_result_unref(self->result);

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
log_db_result_name(gpointer s)
{
  LogDBResult *self = (LogDBResult *) s;

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

LogDBProgram *
log_db_program_new(void)
{
  LogDBProgram *self = g_new0(LogDBProgram, 1);

  self->rules = r_new_node("", NULL);
  self->ref_cnt = 1;
  return self;
}

static LogDBProgram *
log_db_program_ref(LogDBProgram *self)
{
  self->ref_cnt++;
  return self;
}

static void
log_db_program_unref(LogDBProgram *s)
{
  LogDBProgram *self = (LogDBProgram *) s;

  if (--self->ref_cnt == 0)
    {
      if (self->rules)
        r_free_node(self->rules, log_db_result_unref);

      g_free(self);
    }
}


typedef struct _LogDBParserState
{
  LogPatternDatabase *db;
  LogDBProgram *current_program;
  LogDBProgram *root_program;
  LogDBResult *current_result;
  LogDBExample *current_example;
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
} LogDBParserState;

void
log_classifier_xml_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names,
                                    const gchar **attribute_values, gpointer user_data, GError **error)
{
  LogDBParserState *state = (LogDBParserState *) user_data;
  gchar *current_class = NULL;
  gchar *current_rule_id = NULL;
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
      state->current_example = g_new0(LogDBExample, 1);
      state->current_example->result = log_db_result_ref(state->current_result);
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
        msg_error("No name is specified for test_value", evt_tag_str("rule_id", state->current_result->rule_id), NULL);
    }
  else if (strcmp(element_name, "rule") == 0)
    {
      if (state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <rule> element");
          return;
        }

      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "class") == 0)
            current_class = g_strdup(attribute_values[i]);
          else if (strcmp(attribute_names[i], "id") == 0)
            current_rule_id = g_strdup(attribute_values[i]);
        }

      if (!current_rule_id)
        {
          *error = g_error_new(1, 0, "No id attribute for rule element");
          return;
        }

      state->in_rule = TRUE;
      state->current_result = log_db_result_new(current_class, current_rule_id);
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
        msg_error("No name is specified for value", evt_tag_str("rule_id", state->current_result->rule_id), NULL);
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
log_classifier_xml_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  LogDBParserState *state = (LogDBParserState *) user_data;

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
        log_pattern_example_free(state->current_example);

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
      if (state->current_result)
        {
          log_db_result_unref(state->current_result);
          state->current_result = NULL;
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
log_classifier_xml_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  LogDBParserState *state = (LogDBParserState *) user_data;
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
                        log_db_result_ref(state->current_result),
                        TRUE, log_db_result_name);
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
                  state->current_program = log_db_program_new();

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
                            log_db_program_ref(state->current_program),
                            TRUE, NULL);
                }
            }
        }
      g_free(txt);
    }
  else if (state->in_tag)
    {
      tag = log_tags_get_by_name(text);
      g_array_append_val(state->current_result->tags, tag);
    }
  else if (state->value_name)
    {
      if (!state->current_result->values)
        state->current_result->values = g_ptr_array_new();

      value = log_template_new(state->cfg, state->value_name, text);
      if (!log_template_compile(value, &err))
        {
          msg_error("Error compiling value template",
            evt_tag_str("name", state->value_name),
            evt_tag_str("value", text),
            evt_tag_str("error", err->message), NULL);
          g_error_free(err);
          log_template_unref(value);
        }
      else
        g_ptr_array_add(state->current_result->values, value);
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
  .start_element = log_classifier_xml_start_element,
  .end_element = log_classifier_xml_end_element,
  .text = log_classifier_xml_text,
  .passthrough = NULL,
  .error = NULL
};

static NVHandle class_handle = 0;
static NVHandle rule_id_handle = 0;

gboolean
log_pattern_database_lookup(LogPatternDatabase *self, LogMessage *msg, GSList **dbg_list)
{
  RNode *node;
  GArray *matches;
  const gchar *program;
  gssize program_len;

  if (G_UNLIKELY(!self->programs))
    return FALSE;

  program = log_msg_get_value(msg, LM_V_PROGRAM, &program_len);

  node = r_find_node(self->programs, (gchar *) program, (gchar *) program, program_len, NULL);

  if (node)
    {
      LogDBProgram *program = (LogDBProgram *) node->value;

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

          message = log_msg_get_value(msg, LM_V_MESSAGE, &message_len);
          if (G_UNLIKELY(dbg_list))
            msg_node = r_find_node_dbg(program->rules, (gchar *) message, (gchar *) message, message_len, matches, dbg_list);
          else
            msg_node = r_find_node(program->rules, (gchar *) message, (gchar *) message, message_len, matches);

          if (msg_node)
            {
              LogDBResult *verdict = (LogDBResult *) msg_node->value;
              gint i;

              log_msg_set_value(msg, class_handle, verdict->class ? verdict->class : "system", -1);
              log_msg_set_value(msg, rule_id_handle, verdict->rule_id, -1);

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

              if (verdict->tags)
                {
                  for (i = 0; i < verdict->tags->len; i++)
                    log_msg_set_tag_by_id(msg, g_array_index(verdict->tags, LogTagId, i));
                }

              if (verdict->values)
                {
                  GString *result = g_string_sized_new(32);

                  for (i = 0; i < verdict->values->len; i++)
                    {
                      log_template_format(g_ptr_array_index(verdict->values, i), msg, 0, TS_FMT_ISO, NULL, 0, 0, result);
                      log_msg_set_value(msg, log_msg_get_value_handle(((LogTemplate *)g_ptr_array_index(verdict->values, i))->name), result->str, result->len);
                    }
                  g_string_free(result, TRUE);
                }
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
log_pattern_database_load(LogPatternDatabase *self, GlobalConfig *cfg, const gchar *config, GList **examples)
{
  LogDBParserState state;
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
  state.root_program = log_db_program_new();
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
      log_pattern_database_free(self);
    }
  if (dbfile)
    fclose(dbfile);
  if (parse_ctx)
    g_markup_parse_context_free(parse_ctx);
  return success;
}

void
log_pattern_database_free(LogPatternDatabase *self)
{
  if (self->programs)
    r_free_node(self->programs, (GDestroyNotify) log_db_program_unref);
  if (self->version)
    g_free(self->version);
  if (self->pub_date)
    g_free(self->pub_date);
  self->programs = NULL;
  self->version = NULL;
  self->pub_date = NULL;
}

void
log_pattern_database_init(void)
{
  class_handle = log_msg_get_value_handle(".classifier.class");
  rule_id_handle = log_msg_get_value_handle(".classifier.rule_id");
}
