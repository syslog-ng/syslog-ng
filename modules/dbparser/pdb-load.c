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
#include "pdb-error.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

enum PDBLoaderState
{
  PDBL_INITIAL = 0,
  PDBL_PATTERNDB,
  PDBL_RULESET,
  PDBL_RULESET_URL,
  PDBL_RULESET_DESCRIPTION,
  PDBL_RULESET_PATTERN,
  PDBL_RULES,
  PDBL_RULE,
  PDBL_RULE_URL,
  PDBL_RULE_DESCRIPTION,
  PDBL_RULE_PATTERN,
  PDBL_RULE_EXAMPLES,
  PDBL_RULE_EXAMPLE,
  PDBL_RULE_EXAMPLE_TEST_MESSAGE,
  PDBL_RULE_EXAMPLE_TEST_VALUES,
  PDBL_RULE_EXAMPLE_TEST_VALUE,
  PDBL_RULE_ACTIONS,
  PDBL_RULE_ACTION,
  PDBL_RULE_ACTION_CREATE_CONTEXT,

  /* generic states, reused by multiple paths in the XML */
  PDBL_VALUE,
  PDBL_TAG,
  PDBL_MESSAGE,
};

#define PDB_STATE_STACK_MAX_DEPTH 12

typedef struct _PDBStateStack
{
  gint stack[PDB_STATE_STACK_MAX_DEPTH];
  gint top;
} PDBStateStack;

/* arguments passed to the markup parser functions */
typedef struct _PDBLoader
{
  const gchar *filename;
  GMarkupParseContext *context;

  PDBRuleSet *ruleset;
  PDBProgram *root_program;
  PDBProgram *current_program;
  PDBRule *current_rule;
  PDBAction *current_action;
  PDBExample *current_example;
  SyntheticMessage *current_message;
  enum PDBLoaderState current_state;
  PDBStateStack state_stack;
  gboolean first_program;
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


static void
_pdb_state_stack_push(PDBStateStack *self, gint state)
{
  g_assert(self->top < PDB_STATE_STACK_MAX_DEPTH - 1);
  self->stack[self->top] = state;
  self->top++;
}

static gint
_pdb_state_stack_pop(PDBStateStack *self)
{
  g_assert(self->top > 0);
  self->top--;

  return self->stack[self->top];
}

static void G_GNUC_PRINTF(3, 4)
pdb_loader_set_error(PDBLoader *state, GError **error, const gchar *format, ...)
{
  gchar *error_text;
  gchar *error_location;
  gint line_number, col_number;
  va_list va;

  va_start(va, format);
  error_text = g_strdup_vprintf(format, va);
  va_end(va);

  g_markup_parse_context_get_position(state->context, &line_number, &col_number);
  error_location = g_strdup_printf("%s:%d:%d", state->filename, line_number, col_number);

  g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "%s: %s", error_location, error_text);

  g_free(error_text);
  g_free(error_location);
}

static void
_push_state(PDBLoader *state, gint new_state)
{
  _pdb_state_stack_push(&state->state_stack, state->current_state);
  state->current_state = new_state;
}

static void
_pop_state(PDBLoader *state)
{
  state->current_state = _pdb_state_stack_pop(&state->state_stack);
}

static gboolean
_pop_state_for_closing_tag_with_alternatives(PDBLoader *state, const gchar *element_name,
                                             const gchar *expected_element, const gchar *alternatives,
                                             GError **error)
{
  if (strcmp(element_name, expected_element) != 0)
    {
      pdb_loader_set_error(state, error, "Unexpected </%s> tag, expected </%s>%s%s",
                           element_name,
                           expected_element,
                           alternatives ? ", " : "",
                           alternatives);
      return FALSE;
    }
  _pop_state(state);
  return TRUE;
}

static gboolean
_pop_state_for_closing_tag(PDBLoader *state, const gchar *element_name,
                           const gchar *expected_element,
                           GError **error)
{
  return _pop_state_for_closing_tag_with_alternatives(state, element_name, expected_element, NULL, error);
}

static gboolean
_is_whitespace_only(const gchar *text, gsize text_len)
{
  gint i;

  for (i = 0; i < text_len; i++)
    {
      if (!g_ascii_isspace(text[i]))
        return FALSE;
    }
  return TRUE;
}


static void
_process_value_element(PDBLoader *state,
                       const gchar **attribute_names, const gchar **attribute_values,
                       GError **error)
{
  if (attribute_names[0] && g_str_equal(attribute_names[0], "name"))
    state->value_name = g_strdup(attribute_values[0]);
  else
    {
      pdb_loader_set_error(state, error, "<value> misses name attribute in rule %s", state->current_rule->rule_id);
      return;
    }
  _push_state(state, PDBL_VALUE);
}

static void
_process_tag_element(PDBLoader *state,
                     const gchar **attribute_names, const gchar **attribute_values,
                     GError **error)
{
  _push_state(state, PDBL_TAG);
}

static void
_process_message_element(PDBLoader *state,
                         const gchar **attribute_names, const gchar **attribute_values,
                         SyntheticMessage *target,
                         GError **error)
{
  gint i;

  for (i = 0; attribute_names[i]; i++)
    {
      if (strcmp(attribute_names[i], "inherit-properties") == 0)
        synthetic_message_set_inherit_properties_string(target, attribute_values[i], error);
      else if (strcmp(attribute_names[i], "inherit-mode") == 0)
        synthetic_message_set_inherit_mode_string(target, attribute_values[i], error);
    }
  state->current_message = target;
  _push_state(state, PDBL_MESSAGE);
}

static void
_process_create_context_element(PDBLoader *state,
                                const gchar **attribute_names, const gchar **attribute_values,
                                SyntheticContext *target,
                                GError **error)
{
  gint i;

  for (i = 0; attribute_names[i]; i++)
    {
      if (strcmp(attribute_names[i], "context-id") == 0)
        {
          LogTemplate *template;
          GError *local_error = NULL;

          template = log_template_new(state->cfg, NULL);
          if (!log_template_compile(template, attribute_values[i], &local_error))
            {
              log_template_unref(template);
              pdb_loader_set_error(state, error,
                                   "Error compiling create-context context-id, rule=%s, context-id=%s, error=%s",
                                   state->current_rule->rule_id, attribute_values[i], local_error->message);
              g_clear_error(&local_error);
              return;
            }
          else
            synthetic_context_set_context_id_template(target, template);
        }
      else if (strcmp(attribute_names[i], "context-timeout") == 0)
        synthetic_context_set_context_timeout(target, strtol(attribute_values[i], NULL, 0));
      else if (strcmp(attribute_names[i], "context-scope") == 0)
        synthetic_context_set_context_scope(target, attribute_values[i], error);
    }
  if (!target->id_template)
    {
      pdb_loader_set_error(state, error,
                           "context-id attribute is missing from <create-context>, rule=%s",
                           state->current_rule->rule_id);
      return;
    }
  _push_state(state, PDBL_RULE_ACTION_CREATE_CONTEXT);
}

/* PDBL_INITIAL */

static void
_pdbl_initial_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                    const gchar **attribute_values, GError **error)
{
  gint i;

  if (strcmp(element_name, "patterndb") == 0)
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
          msg_warning("patterndb version is unspecified, assuming v4 format");
          state->ruleset->version = g_strdup("4");
        }
      else if (state->ruleset->version && atoi(state->ruleset->version) < 2)
        {
          pdb_loader_set_error(state, error,
                               "patterndb version too old, this version of syslog-ng only supports v3 and v4 formatted patterndb files, please upgrade it using pdbtool");
          return;
        }
      else if (state->ruleset->version && atoi(state->ruleset->version) > 5)
        {
          pdb_loader_set_error(state, error,
                               "patterndb version too new, this version of syslog-ng supports v3, v4 & v5 formatted patterndb files.");
          return;
        }
      _push_state(state, PDBL_PATTERNDB);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <patterndb>", element_name);
    }
}

/* PDBL_PATTERNDB */

static void
_pdbl_patterndb_start(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (strcmp(element_name, "ruleset") == 0)
    {
      state->ruleset->is_empty = FALSE;
      state->first_program = TRUE;
      state->program_patterns = g_array_new(0, 0, sizeof(PDBProgramPattern));
      _push_state(state, PDBL_RULESET);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <ruleset>", element_name);
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

static void
_pdbl_patterndb_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (_pop_state_for_closing_tag(state, element_name, "patterndb", error))
    {
      g_hash_table_foreach(state->ruleset_patterns, _populate_ruleset_radix, state);
      g_hash_table_remove_all(state->ruleset_patterns);
    }
}

/* PDBL_RULESET */

static void
_pdbl_ruleset_start(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (strcmp(element_name, "rules") == 0)
    {
      _push_state(state, PDBL_RULES);
    }
  else if (strcmp(element_name, "patterns") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "urls") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "url") == 0)
    {
      _push_state(state, PDBL_RULESET_URL);
    }
  else if (strcmp(element_name, "pattern") == 0)
    {
      _push_state(state, PDBL_RULESET_PATTERN);
    }
  else if (strcmp(element_name, "description") == 0)
    {
      _push_state(state, PDBL_RULESET_DESCRIPTION);
    }
  else
    {
      pdb_loader_set_error(state, error,
                           "Unexpected <%s> tag, expected a <rules>, <urls>, <url>, <description>, <patterns> or <pattern>", element_name);
    }
}

static void
_pdbl_ruleset_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  gint i;
  PDBProgramPattern *program_pattern;
  PDBProgram *program;

  if (strcmp(element_name, "patterns") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "urls") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (_pop_state_for_closing_tag_with_alternatives(state, element_name, "ruleset", "</patterns> or </urls>", error))
    {
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

      g_array_free(state->program_patterns, TRUE);
      state->program_patterns = NULL;
    }
}

/* PDBL_RULESET_PATTERN */

static gboolean
_pdbl_ruleset_pattern_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
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
          pdb_loader_set_error(state, error, "Joining rulesets with mismatching program name sets, program=%s", text);
          return FALSE;
        }
    }
  return TRUE;
}

/* PDBL_RULES */

static void
_pdbl_rules_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                  const gchar **attribute_values, GError **error)
{
  gint i;

  if (strcmp(element_name, "rule") == 0)
    {
      if (state->current_rule)
        pdb_rule_unref(state->current_rule);

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
              GError *local_error = NULL;

              template = log_template_new(state->cfg, NULL);
              if (!log_template_compile(template, attribute_values[i], &local_error))
                {
                  log_template_unref(template);
                  pdb_loader_set_error(state, error,
                                       "Error compiling context-id template, rule=%s, context-id=%s, error=%s",
                                       state->current_rule->rule_id, attribute_values[i], local_error->message);
                  g_clear_error(&local_error);
                  return;
                }
              else
                synthetic_context_set_context_id_template(&state->current_rule->context, template);
            }
          else if (strcmp(attribute_names[i], "context-timeout") == 0)
            synthetic_context_set_context_timeout(&state->current_rule->context, strtol(attribute_values[i], NULL, 0));
          else if (strcmp(attribute_names[i], "context-scope") == 0)
            synthetic_context_set_context_scope(&state->current_rule->context, attribute_values[i], error);
        }

      if (!state->current_rule->rule_id)
        {
          pdb_loader_set_error(state, error, "No id attribute for rule element");
          pdb_rule_unref(state->current_rule);
          state->current_rule = NULL;
          return;
        }

      state->current_message = &state->current_rule->msg;
      state->action_id = 0;
      _push_state(state, PDBL_RULE);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <rule>", element_name);
    }
}

/* PDBL_RULE */

static void
_pdbl_rule_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                 const gchar **attribute_values, GError **error)
{
  if (strcmp(element_name, "patterns") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "pattern") == 0)
    {
      _push_state(state, PDBL_RULE_PATTERN);
    }
  else if (strcmp(element_name, "tags") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "tag") == 0)
    {
      _process_tag_element(state, attribute_names, attribute_values, error);
    }
  else if (strcmp(element_name, "values") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "urls") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "description") == 0)
    {
      _push_state(state, PDBL_RULE_DESCRIPTION);
    }
  else if (strcmp(element_name, "url") == 0)
    {
      _push_state(state, PDBL_RULE_URL);
    }
  else if (strcmp(element_name, "value") == 0)
    {
      _process_value_element(state, attribute_names, attribute_values, error);
    }
  else if (strcmp(element_name, "actions") == 0)
    {
      _push_state(state, PDBL_RULE_ACTIONS);
    }
  else if (strcmp(element_name, "examples") == 0)
    {
      _push_state(state, PDBL_RULE_EXAMPLES);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <patterns>, <pattern>, <tags>, <tag> or <actions>",
                           element_name);
    }
}

static void
_pdbl_rule_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (strcmp(element_name, "patterns") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "description") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "tags") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "urls") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "values") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (_pop_state_for_closing_tag_with_alternatives(state, element_name, "rule",
                                                        "</patterns>, </description>, </tags>, </urls>, </values>", error) == 0)
    {
      if (state->current_rule)
        {
          pdb_rule_unref(state->current_rule);
          state->current_rule = NULL;
        }
      state->current_message = NULL;
    }
}

/* PDBL_RULE_EXAMPLES */

static void
_pdbl_rule_examples_start(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (strcmp(element_name, "example") == 0)
    {
      state->current_example = g_new0(PDBExample, 1);
      state->current_example->rule = pdb_rule_ref(state->current_rule);
      _push_state(state, PDBL_RULE_EXAMPLE);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <example>", element_name);
    }
}

/* PDBL_RULE_EXAMPLE */

static void
_pdbl_rule_example_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                         const gchar **attribute_values, GError **error)
{
  gint i;

  if (strcmp(element_name, "test_message") == 0)
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "program") == 0)
            state->current_example->program = g_strdup(attribute_values[i]);
        }
      _push_state(state, PDBL_RULE_EXAMPLE_TEST_MESSAGE);
    }
  else if (strcmp(element_name, "test_values") == 0)
    {
      _push_state(state, PDBL_RULE_EXAMPLE_TEST_VALUES);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <test_message>, <test_values>", element_name);
    }
}

static void
_pdbl_rule_example_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (_pop_state_for_closing_tag(state, element_name, "example", error))
    {
      if (state->load_examples)
        state->examples = g_list_prepend(state->examples, state->current_example);
      else
        pdb_example_free(state->current_example);

      state->current_example = NULL;
    }
}

/* PDBL_RULE_EXAMPLE_TEST_MESSAGE */

static gboolean
_pdbl_rule_example_test_message_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
{
  state->current_example->message = g_strdup(text);
  return TRUE;
}

/* PDBL_RULE_EXAMPLE_TEST_VALUES */

static void
_pdbl_rule_example_test_values_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                                     const gchar **attribute_values, GError **error)
{
  if (strcmp(element_name, "test_value") == 0)
    {
      if (attribute_names[0] && g_str_equal(attribute_names[0], "name"))
        state->test_value_name = g_strdup(attribute_values[0]);
      else
        {
          msg_error("No name is specified for test_value",
                    evt_tag_str("rule_id", state->current_rule->rule_id));
          pdb_loader_set_error(state, error, "<test_value> misses name attribute");
          return;
        }
      _push_state(state, PDBL_RULE_EXAMPLE_TEST_VALUE);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected <test_value>", element_name);
    }
}

/* PDBL_RULE_EXAMPLE_TEST_VALUE */

static void
_pdbl_rule_example_test_value_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (_pop_state_for_closing_tag(state, element_name, "test_value", error))
    {
      if (state->test_value_name)
        g_free(state->test_value_name);

      state->test_value_name = NULL;
    }
}

static gboolean
_pdbl_rule_example_test_value_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
{
  gchar **nv;

  if (!state->current_example->values)
    state->current_example->values = g_ptr_array_new();

  nv = g_new(gchar *, 2);
  nv[0] = state->test_value_name;
  state->test_value_name = NULL;
  nv[1] = g_strdup(text);

  g_ptr_array_add(state->current_example->values, nv);

  return TRUE;
}


/* PDBL_RULE_ACTIONS */

static void
_pdbl_rule_actions_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                         const gchar **attribute_values, GError **error)
{
  gint i;

  if (strcmp(element_name, "action") == 0)
    {
      if (!state->current_rule)
        {
          pdb_loader_set_error(state, error, "Unexpected <action> element, it must be inside a rule");
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
      _push_state(state, PDBL_RULE_ACTION);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <action>", element_name);
    }
}

/* PDBL_RULE_ACTION */

static void
_pdbl_rule_action_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                        const gchar **attribute_values, GError **error)
{
  if (strcmp(element_name, "message") == 0)
    {
      state->current_action->content_type = RAC_MESSAGE;
      _process_message_element(state, attribute_names, attribute_values, &state->current_action->content.message, error);
    }
  else if (strcmp(element_name, "create-context") == 0)
    {
      state->current_action->content_type = RAC_CREATE_CONTEXT;
      _process_create_context_element(state, attribute_names, attribute_values,
                                      &state->current_action->content.create_context.context, error);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected <message> or <create-context>", element_name);
    }
}

/* PDBL_RULE_ACTION_CREATE_CONTEXT */

static void
_pdbl_rule_action_create_context_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                                       const gchar **attribute_values, GError **error)
{
  if (strcmp(element_name, "message") == 0)
    {
      _process_message_element(state, attribute_names, attribute_values,
                               &state->current_action->content.create_context.message, error);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected <message>", element_name);
    }
}

/* PDBL_MESSAGE */

static void
_pdbl_message_start(PDBLoader *state, const gchar *element_name, const gchar **attribute_names,
                    const gchar **attribute_values, GError **error)
{
  if (strcmp(element_name, "values") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "value") == 0)
    {
      _process_value_element(state, attribute_names, attribute_values, error);
    }
  else if (strcmp(element_name, "tags") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "tag") == 0)
    {
      _process_tag_element(state, attribute_names, attribute_values, error);
    }
  else
    {
      pdb_loader_set_error(state, error, "Unexpected <%s> tag, expected a <values>, <value>, <tags> or <tag>", element_name);
    }
}

/* PDBL_RULE_PATTERN */

static gboolean
_pdbl_rule_pattern_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
{
  PDBProgramPattern program_pattern;

  program_pattern.pattern = g_strdup(text);
  program_pattern.rule = pdb_rule_ref(state->current_rule);
  g_array_append_val(state->program_patterns, program_pattern);

  return TRUE;
}

/* PDBL_RULE_ACTION */

static void
_pdbl_rule_action_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (_pop_state_for_closing_tag(state, element_name, "action", error))
    {
      pdb_rule_add_action(state->current_rule, state->current_action);
      state->current_action = NULL;
    }
}

/* PDBL_MESSAGE */

static void
_pdbl_message_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (strcmp(element_name, "values") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (strcmp(element_name, "tags") == 0)
    {
      /* valid, but we don't do anything */
    }
  else if (_pop_state_for_closing_tag_with_alternatives(state, element_name, "message", "</values>, </tags>", error))
    {
      state->current_message = &state->current_rule->msg;
    }
}

/* PDBL_VALUE */

static void
_pdbl_value_end(PDBLoader *state, const gchar *element_name, GError **error)
{
  if (_pop_state_for_closing_tag(state, element_name, "value", error))
    {
      if (state->value_name)
        g_free(state->value_name);

      state->value_name = NULL;
    }
}

static gboolean
_pdbl_value_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
{
  GError *err = NULL;

  g_assert(state->value_name != NULL);
  if (!synthetic_message_add_value_template_string(state->current_message, state->cfg, state->value_name, text, &err))
    {
      pdb_loader_set_error(state, error, "Error compiling value template, rule=%s, name=%s, value=%s, error=%s",
                           state->current_rule->rule_id, state->value_name, text, err->message);
      return FALSE;
    }

  return TRUE;
}

/* PDBL_TAG */

static gboolean
_pdbl_tag_text(PDBLoader *state, const gchar *text, gsize text_len, GError **error)
{
  synthetic_message_add_tag(state->current_message, text);

  return TRUE;
}

/* element start callback */

void
pdb_loader_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names,
                         const gchar **attribute_values, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;

  switch (state->current_state)
    {
    case PDBL_INITIAL:
      _pdbl_initial_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_PATTERNDB:
      _pdbl_patterndb_start(state, element_name, error);
      break;
    case PDBL_RULESET:
      _pdbl_ruleset_start(state, element_name, error);
      break;
    case PDBL_RULES:
      _pdbl_rules_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE:
      _pdbl_rule_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE_EXAMPLES:
      _pdbl_rule_examples_start(state, element_name, error);
      break;
    case PDBL_RULE_EXAMPLE:
      _pdbl_rule_example_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE_EXAMPLE_TEST_VALUES:
      _pdbl_rule_example_test_values_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE_ACTIONS:
      _pdbl_rule_actions_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE_ACTION:
      _pdbl_rule_action_start(state, element_name, attribute_names, attribute_values, error);
      break;
    case PDBL_RULE_ACTION_CREATE_CONTEXT:
      _pdbl_rule_action_create_context_start(state, element_name, attribute_names, attribute_values, error);
      break;

    /* generic states reused by multiple locations in the grammar */

    case PDBL_MESSAGE:
      _pdbl_message_start(state, element_name, attribute_names, attribute_values, error);
      break;
    default:
      pdb_loader_set_error(state, error, "Unexpected state %d, tag <%s>", state->current_state, element_name);
      break;
    }
}

void
pdb_loader_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;

  switch (state->current_state)
    {
    case PDBL_PATTERNDB:
      _pdbl_patterndb_end(state, element_name, error);
      break;
    case PDBL_RULESET:
      _pdbl_ruleset_end(state, element_name, error);
      break;
    case PDBL_RULESET_URL:
      _pop_state_for_closing_tag(state, element_name, "url", error);
      break;
    case PDBL_RULESET_PATTERN:
      _pop_state_for_closing_tag(state, element_name, "pattern", error);
      break;
    case PDBL_RULESET_DESCRIPTION:
      _pop_state_for_closing_tag(state, element_name, "description", error);
      break;
    case PDBL_RULES:
      _pop_state_for_closing_tag(state, element_name, "rules", error);
      break;
    case PDBL_RULE:
      _pdbl_rule_end(state, element_name, error);
      break;
    case PDBL_RULE_DESCRIPTION:
      _pop_state_for_closing_tag(state, element_name, "description", error);
      break;
    case PDBL_RULE_URL:
      _pop_state_for_closing_tag(state, element_name, "url", error);
      break;
    case PDBL_RULE_PATTERN:
      _pop_state_for_closing_tag(state, element_name, "pattern", error);
      break;
    case PDBL_RULE_EXAMPLES:
      _pop_state_for_closing_tag(state, element_name, "examples", error);
      break;
    case PDBL_RULE_EXAMPLE:
      _pdbl_rule_example_end(state, element_name, error);
      break;
    case PDBL_RULE_EXAMPLE_TEST_VALUES:
      _pop_state_for_closing_tag(state, element_name, "test_values", error);
      break;
    case PDBL_RULE_EXAMPLE_TEST_VALUE:
      _pdbl_rule_example_test_value_end(state, element_name, error);
      break;
    case PDBL_RULE_EXAMPLE_TEST_MESSAGE:
      _pop_state_for_closing_tag(state, element_name, "test_message", error);
      break;
    case PDBL_RULE_ACTIONS:
      _pop_state_for_closing_tag(state, element_name, "actions", error);
      break;
    case PDBL_RULE_ACTION:
      _pdbl_rule_action_end(state, element_name, error);
      break;
    case PDBL_RULE_ACTION_CREATE_CONTEXT:
      _pop_state_for_closing_tag(state, element_name, "create-context", error);
      break;
    /* generic states reused by multiple locations in the grammar */
    case PDBL_MESSAGE:
      _pdbl_message_end(state, element_name, error);
      break;

    case PDBL_VALUE:
      _pdbl_value_end(state, element_name, error);
      break;

    case PDBL_TAG:
      _pop_state_for_closing_tag(state, element_name, "tag", error);
      break;

    default:
      pdb_loader_set_error(state, error, "Unexpected state %d, tag </%s>", state->current_state, element_name);
      break;
    }
}

void
pdb_loader_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  PDBLoader *state = (PDBLoader *) user_data;

  switch (state->current_state)
    {
    case PDBL_RULESET_DESCRIPTION:
      break;
    case PDBL_RULESET_PATTERN:
      if (!_pdbl_ruleset_pattern_text(state, text, text_len, error))
        return;
      break;
    case PDBL_RULESET_URL:
      break;
    case PDBL_RULE_PATTERN:
      if (!_pdbl_rule_pattern_text(state, text, text_len, error))
        return;
      break;
    case PDBL_RULE_EXAMPLE:
      break;
    case PDBL_RULE_URL:
      break;
    case PDBL_RULE_DESCRIPTION:
      break;
    case PDBL_RULE_EXAMPLE_TEST_MESSAGE:
      if (!_pdbl_rule_example_test_message_text(state, text, text_len, error))
        return;
      break;
    case PDBL_RULE_EXAMPLE_TEST_VALUE:
      if (!_pdbl_rule_example_test_value_text(state, text, text_len, error))
        return;
      break;

    /* generic states reused by multiple locations in the grammar */

    case PDBL_VALUE:
      if (!_pdbl_value_text(state, text, text_len, error))
        return;
      break;

    case PDBL_TAG:
      if (!_pdbl_tag_text(state, text, text_len, error))
        return;
      break;

    default:
      if (!_is_whitespace_only(text, text_len))
        pdb_loader_set_error(state, error, "Unexpected text node in state %d, text=[[%s]]", state->current_state, text);
      break;
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
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }

  memset(&state, 0x0, sizeof(state));

  state.ruleset = self;
  state.root_program = pdb_program_new();
  state.load_examples = !!examples;
  state.ruleset_patterns = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) pdb_program_unref);
  state.cfg = cfg;
  state.filename = config;
  state.context = parse_ctx = g_markup_parse_context_new(&db_parser, 0, &state, NULL);

  self->programs = r_new_node("", state.root_program);

  while ((bytes_read = fread(buff, sizeof(gchar), 4096, dbfile)) != 0)
    {
      if (!g_markup_parse_context_parse(parse_ctx, buff, bytes_read, &error))
        {
          msg_error("Error parsing pattern database file",
                    evt_tag_str(EVT_TAG_FILENAME, config),
                    evt_tag_str("error", error ? error->message : "unknown"));
          goto error;
        }
    }
  fclose(dbfile);
  dbfile = NULL;

  if (!g_markup_parse_context_end_parse(parse_ctx, &error))
    {
      msg_error("Error parsing pattern database file",
                evt_tag_str(EVT_TAG_FILENAME, config),
                evt_tag_str("error", error ? error->message : "unknown"));
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
