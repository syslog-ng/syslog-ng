/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "logpatterns.h"
#include "logmsg.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

LogDBResult *
log_db_result_new(gchar *class, gchar *rule_id)
{
  LogDBResult *self = g_new0(LogDBResult, 1);

  self->class = class;
  self->rule_id = rule_id;

  return self;
}

static void
log_db_result_free(void *s)
{
  LogDBResult *self = (LogDBResult *) s;

  if (self->rule_id)
    g_free(self->rule_id);

  if (self->class)
    g_free(self->class);

  g_free(self);
}


/*
 * Database based parser. The patterns are stored in an XML database.
 * Data structure is: 
 *   - Parser -> programs -> rules
 */

/*
 * This class encapsulates a set of program related rules in the
 * pattern database. Its instances are stored as "value" in the
 * program name RADIX tree. It basically contains another RADIX for
 * the per-program patterns.
 */
typedef struct _LogDBProgram
{
  RNode *rules;
  gchar *name;
} LogDBProgram;


LogDBProgram *
log_db_program_new(const gchar *name)
{
  LogDBProgram *self = g_new0(LogDBProgram, 1);

  self->rules = r_new_node("", NULL);

  return self;
}

static void
log_db_program_free(void *s)
{
  LogDBProgram *self = (LogDBProgram *) s;

  if (self->rules)
    r_free_node(self->rules, log_db_result_free);

  if (self->name)
    g_free(self->name);

  g_free(self);
}


typedef struct _LogDBParserState
{
  LogPatternDatabase *db;
  LogDBProgram *current_program;
  gchar *current_class;
  gchar *current_rule_id;
  gboolean in_pattern;
} LogDBParserState;

void
log_classifier_xml_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names,
                                    const gchar **attribute_values, gpointer user_data, GError **error)
{
  LogDBParserState *state = (LogDBParserState *) user_data;
  gint i;

  if (strcmp(element_name, "program") == 0)
    {
      if (state->current_program)
        {
          *error = g_error_new(1, 1, "Unexpected <program> element");
          return;
        }

      for (i = 0; attribute_names[i]; i++)
        if (strcmp(attribute_names[i], "name") == 0)
          break;

      if (!attribute_values[i])
        {
          *error = g_error_new(1, 0, "No name attribute for program element");
          return;
        }

      state->current_program = log_db_program_new(attribute_values[i]);
    }
  else if (strcmp(element_name, "rule") == 0)
    {
      if (!state->current_program)
        {
          *error = g_error_new(1, 0, "Unexpected <rule> element");
          return;
        }

      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp(attribute_names[i], "class") == 0)
            state->current_class = g_strdup(attribute_values[i]);
          else if (strcmp(attribute_names[i], "id") == 0)
            state->current_rule_id = g_strdup(attribute_values[i]);
        }

      if (!state->current_class)
        {
          *error = g_error_new(1, 0, "No class attribute for rule element");
          return;
        }
      if (!state->current_rule_id)
        {
          *error = g_error_new(1, 0, "No id attribute for rule element");
          return;
        }

    }
  else if (strcmp(element_name, "pattern") == 0)
    {
      state->in_pattern = TRUE;
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

  if (strcmp(element_name, "program") == 0)
    {
      if (!state->current_program)
        {
          *error = g_error_new(1, 0, "Unexpected </program> element");
          return;
        }

      state->current_program = NULL;
    }
  else if (strcmp(element_name, "rule") == 0)
    {
      if (state->current_class)
        {
          g_free(state->current_class);
          state->current_class = NULL;
        }

      if (state->current_rule_id)
        {
          g_free(state->current_rule_id);
          state->current_rule_id = NULL;
        }
    }
  else if (strcmp(element_name, "pattern") == 0)
    state->in_pattern = FALSE;
}

void
log_classifier_xml_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  LogDBParserState *state = (LogDBParserState *) user_data;
  gchar *txt;

  if (!state->in_pattern)
    return;

  txt = g_strdup(text);

  if (state->current_rule_id)
    {
      r_insert_node(state->current_program->rules,
                    txt,
                    log_db_result_new(state->current_class, state->current_rule_id),
                    TRUE);
      state->current_rule_id = NULL;
      state->current_class = NULL;
    }
  else if (state->current_program)
    {
      r_insert_node(state->db->programs,
                    txt,
                    state->current_program,
                    TRUE);
    }
  g_free(txt);
}

GMarkupParser db_parser =
{
  .start_element = log_classifier_xml_start_element,
  .end_element = log_classifier_xml_end_element,
  .text = log_classifier_xml_text,
  .passthrough = NULL,
  .error = NULL
};

LogDBResult *
log_pattern_database_lookup(LogPatternDatabase *self, LogMessage *msg)
{
  RNode *node;
  GArray *matches;
  GPtrArray *match_names;

  if (G_UNLIKELY(!self->programs))
    return NULL;
  node = r_find_node(self->programs, msg->program, msg->program, msg->program_len, NULL, NULL);

  if (node)
    {
      LogDBProgram *program = (LogDBProgram *) node->value;

      if (program->rules)
        {
          RNode *msg_node;

          /* NOTE: We're not using g_array_sized_new as that does not
           * correctly zero-initialize the new items even if clear_ is TRUE
           */
          
          matches = g_array_new(FALSE, TRUE, sizeof(LogMessageMatch));
          g_array_set_size(matches, 1);
          match_names = g_ptr_array_new();
          g_ptr_array_set_size(match_names, 1);

          msg_node = r_find_node(program->rules, msg->message, msg->message, msg->message_len, matches, match_names);

          if (msg_node)
            {
              gint i;

              for (i = 0; i < matches->len; i++)
                {
                  LogMessageMatch *match = &g_array_index(matches, LogMessageMatch, i);
                  gchar *match_name = g_ptr_array_index(match_names, i);
                  gchar *value;

                  if (!match->match)
                    continue;
                  if (match->flags & LMM_REF_MATCH)
                    match->builtin_value = LM_F_MESSAGE;
                    
                  if (match_name && match_name[0])
                    {
                      if ((match->flags & LMM_REF_MATCH) != 0)
                        value = g_strndup(msg->message + match->ofs, match->len);
                      else
                        value = g_strdup(match->match);
                      log_msg_add_dyn_value_ref(msg, match_name, value);
                    }
                }
              log_msg_set_matches(msg, matches->len, (LogMessageMatch *) matches->data);

              /* NOTE: the area pointed to by match_names elements is
               * passed as a reference above, so no need to free it 
               */

              g_ptr_array_free(match_names, TRUE);
              return ((LogDBResult *) msg_node->value);
            }
          g_ptr_array_free(match_names, TRUE);
          g_array_free(matches, TRUE);
        }
    }
  return NULL;
}

gboolean
log_pattern_database_load(LogPatternDatabase *self, const gchar *config)
{
  LogDBParserState state;
  GMarkupParseContext *parse_ctx = NULL;
  GError *error = NULL;
  FILE *cfg = NULL;
  gint bytes_read;
  gchar buff[4096];
  gboolean success = FALSE;

  if ((cfg = fopen(config, "r")) == NULL)
    {
      msg_error("Error opening classifier configuration file",
                 evt_tag_str(EVT_TAG_FILENAME, config),
                 evt_tag_errno(EVT_TAG_OSERROR, errno),
                 NULL);
      goto error;
    }

  self->programs = r_new_node("", NULL);

  state.db = self;
  state.current_program = NULL;
  state.current_rule_id = NULL; 
  state.in_pattern = FALSE;

  parse_ctx = g_markup_parse_context_new(&db_parser, 0, &state, NULL);

  while ((bytes_read = fread(buff, sizeof(gchar), 4096, cfg)) != 0)
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
  fclose(cfg);
  cfg = NULL;

  if (!g_markup_parse_context_end_parse(parse_ctx, &error))
    {
      msg_error("Error parsing pattern database file",
                evt_tag_str(EVT_TAG_FILENAME, config),
                evt_tag_str("error", error ? error->message : "unknown"),
                NULL);
      goto error;
    }

  success = TRUE;

 error:
  if (!success)
    {
      log_pattern_database_free(self);
    }
  if (cfg)
    fclose(cfg);
  if (parse_ctx)
    g_markup_parse_context_free(parse_ctx);
  return success;
}

void
log_pattern_database_free(LogPatternDatabase *self)
{
  if (self->programs)
    r_free_node(self->programs, log_db_program_free);
  if (self->version)
    g_free(self->version);
  if (self->pub_date)
    g_free(self->pub_date);
  self->programs = NULL;
  self->version = NULL;
  self->pub_date = NULL;
}
