/*
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
 */
#include "smart-multi-line.h"
#include "regexp-multi-line.h"
#include "reloc.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum
{
  SMLS_NONE,
  SMLS_START_STATE,
};

typedef struct _SmartMultiLineRule SmartMultiLineRule;
struct _SmartMultiLineRule
{
  gint from_states[4];
  gchar *regexp;
  gint to_state;
  MultiLinePattern *compiled_regexp;
};

typedef struct _SmartMultiLine
{
  MultiLineLogic super;
  GMutex lock;
  gint current_state;
  gboolean last_segment_rewound;
  gboolean rewound_segment_is_trace;
  gboolean consumed_message_is_trace;
} SmartMultiLine;

GHashTable *state_map;
gint last_state_id = SMLS_START_STATE;
GArray *rules;
GPtrArray *rules_by_from_state[64];

static void
_reshuffle_rules_by_from_state(void)
{
  for (gint rule_ndx = 0; rule_ndx < rules->len; rule_ndx++)
    {
      SmartMultiLineRule *rule = &g_array_index(rules, SmartMultiLineRule, rule_ndx);

      rule->compiled_regexp = multi_line_pattern_compile(rule->regexp, NULL);
      g_assert(rule->compiled_regexp != NULL);

      for (gint i = 0; rule->from_states[i]; i++)
        {
          g_assert(i < G_N_ELEMENTS(rule->from_states));

          gint from_state = rule->from_states[i];

          if (rules_by_from_state[from_state] == NULL)
            rules_by_from_state[from_state] = g_ptr_array_new();

          g_ptr_array_add(rules_by_from_state[from_state], rule);
        }
    }
}

static gint
_map_state(const gchar *state)
{
  if (!state_map)
    {
      state_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
      g_hash_table_insert(state_map, g_strdup("start_state"), GINT_TO_POINTER(SMLS_START_STATE));
    }

  gpointer value = g_hash_table_lookup(state_map, state);

  if (value)
    return GPOINTER_TO_INT(value);

  if (last_state_id >= G_N_ELEMENTS(rules_by_from_state))
    {
      msg_error("smart-multi-line: too many states used in smart-multi-line.fsm, running with a partial a rule-set",
                evt_tag_int("fsm-max-state", G_N_ELEMENTS(rules_by_from_state)),
                evt_tag_str("state", state));
      return 0;
    }

  last_state_id++;
  g_hash_table_insert(state_map, g_strdup(state), GINT_TO_POINTER(last_state_id));
  return last_state_id;
}

static gchar *
_extract_regexp(const gchar *regexp)
{
  gint len = strlen(regexp);
  if (regexp[0] == regexp[len - 1])
    return g_strndup(&regexp[1], len - 2);
  return g_strdup(regexp);
}

static void
_parse_rule(const gchar *from_states, const gchar *regexp, const gchar *to_state)
{
  SmartMultiLineRule new_rule = {0};

  gchar **from_state_list = g_strsplit(from_states, ",", -1);

  /* leave one slot for a terminating element */
  for (gint i = 0; from_state_list[i] && i < G_N_ELEMENTS(new_rule.from_states) - 1; i++)
    new_rule.from_states[i] = _map_state(from_state_list[i]);

  new_rule.regexp = _extract_regexp(regexp);
  new_rule.to_state = _map_state(to_state);
  g_strfreev(from_state_list);
  g_array_append_val(rules, new_rule);
}

static void
_read_rules(const gchar *filename, FILE *sml_file)
{
  gchar line[1024];
  gint lineno = 0;

  while (fgets(line, sizeof(line), sml_file))
    {
      lineno++;
      gint len = strlen(line);
      if (len > 0 && line[len - 1] == '\n')
        {
          line[len - 1] = 0;
          len--;
        }

      if (len == 0 || line[0] == '#')
        continue;

      gchar **parts = g_strsplit(line, "\t", 3);

      if (g_strv_length(parts) != 3)
        {
          msg_error("smart-multi-line: error parsing line in pattern file, lines need to be in the form <from_states>\t<regexp>\t<to_state>",
                    evt_tag_str("filename", filename),
                    evt_tag_int("lineno", lineno));
          continue;
        }

      gchar *from_states = parts[0];
      gchar *regexp = parts[1];
      gchar *to_state = parts[2];

      msg_trace("smart-multi-line: processing pattern",
                evt_tag_str("from_states", from_states),
                evt_tag_str("regexp", regexp),
                evt_tag_str("to_state", to_state));

      _parse_rule(from_states, regexp, to_state);
      g_strfreev(parts);
    }
}

static void
_load_tsv_file(const gchar *sml_file_name)
{
  FILE *sml_file = fopen(sml_file_name, "r");

  if (!sml_file)
    {
      msg_error("smart-multi-line: error opening smart-multi-line.fsm file",
                evt_tag_str("filename", sml_file_name),
                evt_tag_error("error"));
      return;
    }

  _read_rules(sml_file_name, sml_file);
  fclose(sml_file);
}

static void
_load_rules(const gchar *sml_file_name)
{
  if (rules)
    return;

  rules = g_array_new(FALSE, TRUE, sizeof(SmartMultiLineRule));
  _load_tsv_file(sml_file_name);
  _reshuffle_rules_by_from_state();
  if (state_map)
    {
      g_hash_table_unref(state_map);
      state_map = NULL;
    }

  if (rules_by_from_state[SMLS_START_STATE] == NULL)
    {
      msg_warning("smart-multi-line: your smart-multi-line.fsm seems to be empty or non-existent, "
                  "automatic multi-line log extraction will probably not work",
                  evt_tag_str("filename", sml_file_name));
    }
}

static void
_free_rules(void)
{
  for (gint state_ndx = 0; state_ndx < G_N_ELEMENTS(rules_by_from_state); state_ndx++)
    {
      if (rules_by_from_state[state_ndx])
        {
          g_ptr_array_free(rules_by_from_state[state_ndx], TRUE);
          rules_by_from_state[state_ndx] = NULL;
        }
    }

  for (gint rule_ndx = 0; rule_ndx < rules->len; rule_ndx++)
    {
      SmartMultiLineRule *rule = &g_array_index(rules, SmartMultiLineRule, rule_ndx);

      multi_line_pattern_unref(rule->compiled_regexp);
      g_free(rule->regexp);
    }
  g_array_free(rules, TRUE);
  rules = NULL;
}

gboolean
_fsm_transition(SmartMultiLine *self, const gchar *segment, gsize segment_len)
{
  GPtrArray *applicable_rules = rules_by_from_state[self->current_state];

  for (gint i = 0; applicable_rules && i < applicable_rules->len; i++)
    {
      SmartMultiLineRule *rule = g_ptr_array_index(applicable_rules, i);
      gboolean match = multi_line_pattern_match(rule->compiled_regexp, (const guchar *) segment, segment_len);

      msg_trace_printf("smart-multi-line: Matching against pattern: %s in state %d, matched %d", rule->regexp,
                       self->current_state, match);
      if (match)
        {
          self->current_state = rule->to_state;
          /* the current segment is part of a sequence */
          return TRUE;
        }
    }
  self->current_state = SMLS_START_STATE;
  return FALSE;
}

void
_process_segment(SmartMultiLine *self, const gchar *segment, gsize segment_len,
                 gboolean *segment_is_part_of_trace,
                 gboolean *segment_starts_a_new_trace,
                 gboolean *segment_ends_trace)
{
  *segment_is_part_of_trace = FALSE;
  *segment_starts_a_new_trace = FALSE;
  *segment_ends_trace = FALSE;

  gboolean last_segment_ended_the_trace = self->current_state == SMLS_START_STATE;

  gboolean segment_is_trace = _fsm_transition(self, segment, segment_len);

  msg_trace_printf("smart-multi-line: [STEP1] >>%.*s<<, result=%d, state=%d", (int) segment_len, segment,
                   segment_is_trace, self->current_state);
  *segment_is_part_of_trace = segment_is_trace;

  if (!(*segment_is_part_of_trace))
    {
      /* try again from the start state, the current segment is may be part of a new trace */
      segment_is_trace = _fsm_transition(self, segment, segment_len);

      msg_trace_printf("smart-multi-line: [STEP2]: >>%.*s<<, result=%d, state=%d", (int) segment_len, segment,
                       segment_is_trace, self->current_state);

      *segment_is_part_of_trace = segment_is_trace;
      if (*segment_is_part_of_trace)
        *segment_starts_a_new_trace = TRUE;
    }
  else
    {
      if (last_segment_ended_the_trace)
        *segment_starts_a_new_trace = TRUE;
      *segment_ends_trace = (self->current_state == SMLS_START_STATE);
    }
}

static gint
_accumulate_line_unlocked(MultiLineLogic *s,
                          const guchar *msg, gsize msg_len,
                          const guchar *segment, gsize segment_len)
{
  SmartMultiLine *self = (SmartMultiLine *) s;

  if (self->last_segment_rewound)
    {
      /* we rewound the last segment, processing it again.  The previous
       * message was extracted we are starting with a fresh start, but we
       * don't run the current segment through the FSM, we simply reuse the
       * result the last time we've run it.  */

      g_assert(msg_len == 0);

      self->last_segment_rewound = FALSE;
      if (self->rewound_segment_is_trace)
        {
          self->consumed_message_is_trace = TRUE;
          return MLL_CONSUME_SEGMENT | MLL_WAITING;
        }
      else
        {
          self->consumed_message_is_trace = FALSE;
          return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
        }
    }
  else
    {
      gboolean segment_is_part_of_trace, segment_starts_a_new_trace, segment_ends_trace;

      _process_segment(self, (const gchar *) segment, segment_len,
                       &segment_is_part_of_trace,
                       &segment_starts_a_new_trace,
                       &segment_ends_trace);

      if (msg_len == 0)
        {
          /* just starting up, first segment is to be consumed */

          if (!segment_is_part_of_trace)
            {
              /* not recognized as part of a trace, just return it */
              self->consumed_message_is_trace = FALSE;
              return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
            }

          /* first line of the trace, remember that we are within a trace
           * and let's wait for more */
          self->consumed_message_is_trace = TRUE;
          return MLL_CONSUME_SEGMENT | MLL_WAITING;
        }
      else
        {
          /* we already have data in @msg, so processing subsequent lines */

          if (self->consumed_message_is_trace && segment_is_part_of_trace)
            {
              /* ok, @msg is a trace and the new segment as well. */

              if (segment_starts_a_new_trace)
                {
                  /* the new segment is not actually part of the consumed
                   * @msg, it starts a new trace */

                  self->last_segment_rewound = TRUE;
                  self->rewound_segment_is_trace = TRUE;
                  return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
                }
              else if (segment_ends_trace)
                {
                  /* the new segment is trace, and is the last part of @msg */
                  return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
                }
              else
                {
                  /* the new segment is a trace and is part of the partially
                   * consumed @msg */

                  return MLL_CONSUME_SEGMENT | MLL_WAITING;
                }
            }
          else if (self->consumed_message_is_trace && !segment_is_part_of_trace)
            {
              /* ok, we have a consumed trace message in @msg and the new
               * segment is not part of that. */

              self->last_segment_rewound = TRUE;
              self->rewound_segment_is_trace = FALSE;
              self->consumed_message_is_trace = FALSE;
              return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
            }
          else
            {
              /* we can only be here with consumed_message_is_trace == TRUE */
              g_assert_not_reached();
            }
        }
    }
  g_assert_not_reached();
}

static gint
_accumulate_line(MultiLineLogic *s,
                 const guchar *msg, gsize msg_len,
                 const guchar *segment, gsize segment_len)
{
  SmartMultiLine *self = (SmartMultiLine *) s;

  g_mutex_lock(&self->lock);
  gint result = _accumulate_line_unlocked(s, msg, msg_len, segment, segment_len);
  g_mutex_unlock(&self->lock);
  return result;
}

static void
_free(MultiLineLogic *s)
{
  SmartMultiLine *self = (SmartMultiLine *) s;
  g_mutex_clear(&self->lock);
  multi_line_logic_free_method(s);
}

MultiLineLogic *
smart_multi_line_new(void)
{
  SmartMultiLine *self = g_new0(SmartMultiLine, 1);

  multi_line_logic_init_instance(&self->super);
  self->super.free_fn = _free;
  self->super.accumulate_line = _accumulate_line;
  self->last_segment_rewound = FALSE;
  self->current_state = SMLS_START_STATE;
  g_mutex_init(&self->lock);

  return &self->super;
}

void
smart_multi_line_global_init(void)
{
  const gchar *sml_file_name = get_installation_path_for("${pkgdatadir}/smart-multi-line.fsm");

  _load_rules(sml_file_name);
}

void
smart_multi_line_global_deinit(void)
{
  _free_rules();
}
