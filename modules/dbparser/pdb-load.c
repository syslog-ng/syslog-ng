/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
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
#include "pdb-rule.h"
#include "pdb-program.h"
#include "pdb-action.h"
#include "pdb-example.h"
#include "pdb-ruleset.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* arguments passed to the markup parser functions */
typedef struct _PDBLoader
{
  PDBRuleSet *ruleset;
  PDBProgram *root_program;
  PDBProgram *current_program;
  PDBRule *current_rule;
  PDBAction *current_action;
  PDBExample *current_example;
  SyntheticMessage *current_message;
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
  GHashTable *ruleset_patterns;
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
            pdb_action_set_message_inheritance(state->current_action, attribute_values[i], error);
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

static void
_populate_ruleset_radix(gpointer key, gpointer value, gpointer user_data)
{
  PDBLoader *state = (PDBLoader *) user_data;
  gchar *pattern = key;
  PDBProgram *program = (PDBProgram *) value;

  r_insert_node(state->ruleset->programs, pattern, pdb_program_ref(program), NULL);
}

void
pdb_loader_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;
  PDBProgramPattern *program_pattern;
  PDBProgram *program;
  int i;


  if (strcmp(element_name, "patterndb") == 0)
    {
      g_hash_table_foreach(state->ruleset_patterns, _populate_ruleset_radix, state);
      g_hash_table_remove_all(state->ruleset_patterns);
    }
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
                        (RNodeGetValueFunc) pdb_rule_get_name);
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
  GError *err = NULL;
  PDBProgramPattern program_pattern;
  gchar **nv;

  if (state->in_pattern)
    {
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
              state->current_program = g_hash_table_lookup(state->ruleset_patterns, text);
              if (state->current_program == NULL)
                {
                  /* create new program specific radix */
                  state->current_program = pdb_program_new();
                  g_hash_table_insert(state->ruleset_patterns, g_strdup(text), state->current_program);
                }

              state->first_program = FALSE;
            }
          else if (state->current_program)
            {
              /* secondary program names should point to the same MSG radix */

              PDBProgram *program = g_hash_table_lookup(state->ruleset_patterns, text);
              if (!program)
                {
                  g_hash_table_insert(state->ruleset_patterns, g_strdup(text), pdb_program_ref(state->current_program));
                }
              else if (program != state->current_program)
                {
                  *error = g_error_new(1, 0, "Joining rulesets with mismatching program name sets, program=%s", text);
                  return;
                }
            }
        }
    }
  else if (state->in_tag)
    {
      if (!state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <tag> element, must be within a rule");
          return;
        }
      synthetic_message_add_tag(state->current_message, text);
    }
  else if (state->value_name)
    {
      if (!state->in_rule)
        {
          *error = g_error_new(1, 0, "Unexpected <value> element, must be within a rule");
          return;
        }
      if (!synthetic_message_add_value_template_string(state->current_message, state->cfg, state->value_name, text, &err))
        {
          *error = g_error_new(1, 0, "Error compiling value template, rule=%s, name=%s, value=%s, error=%s",
                               state->current_rule->rule_id, state->value_name, text, err->message);
          return;
        }
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
  state.ruleset_patterns = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) pdb_program_unref);
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
  g_hash_table_unref(state.ruleset_patterns);
  return success;
}
