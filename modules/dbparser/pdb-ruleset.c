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
#include "pdb-ruleset.h"
#include "pdb-program.h"
#include "pdb-lookup-params.h"
#include "scratch-buffers.h"

static NVHandle class_handle = 0;
static NVHandle rule_id_handle = 0;
static LogTagId system_tag;
static LogTagId unknown_tag;


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
static void
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

const gchar *
_calculate_program(PDBLookupParams *lookup, LogMessage *msg, gssize *program_len)
{
  if (lookup->program_handle)
    return log_msg_get_value(msg, lookup->program_handle, program_len);

  GString *program = scratch_buffers_alloc();
  log_template_format(lookup->program_template, msg, NULL, LTZ_LOCAL, 0, NULL, program);
  *program_len = program->len;
  return program->str;
}

/*
 * Looks up a matching rule in the ruleset.
 *
 * NOTE: it also modifies @msg to store the name-value pairs found during lookup, so
 */
PDBRule *
pdb_ruleset_lookup(PDBRuleSet *rule_set, PDBLookupParams *lookup, GArray *dbg_list)
{
  RNode *node;
  LogMessage *msg = lookup->msg;
  GArray *prg_matches, *matches;
  const gchar *program_value;
  gssize program_len;

  if (G_UNLIKELY(!rule_set->programs))
    return FALSE;

  program_value = _calculate_program(lookup, msg, &program_len);
  prg_matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
  node = r_find_node(rule_set->programs, (gchar *) program_value, program_len, prg_matches);

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
            msg_node = r_find_node_dbg(program->rules, (gchar *) message, message_len, matches, dbg_list);
          else
            msg_node = r_find_node(program->rules, (gchar *) message, message_len, matches);

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


PDBRuleSet *
pdb_rule_set_new(void)
{
  PDBRuleSet *self = g_new0(PDBRuleSet, 1);
  self->is_empty = TRUE;

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

void
pdb_rule_set_global_init(void)
{
  class_handle = log_msg_get_value_handle(".classifier.class");
  rule_id_handle = log_msg_get_value_handle(".classifier.rule_id");
  system_tag = log_tags_get_by_name(".classifier.system");
  unknown_tag = log_tags_get_by_name(".classifier.unknown");
}
