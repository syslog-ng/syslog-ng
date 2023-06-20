/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

#include "value-pairs/cmdline.h"

typedef enum
{
  TFVP_NAMES,
  TFVP_VALUES,
} TFValuePairsResultType;

typedef struct _TFValuePairsState
{
  TFSimpleFuncState super;
  ValuePairs *vp;
  TFValuePairsResultType result_type;
} TFValuePairsState;

typedef struct _TFValuePairsIterState
{
  GString *result;
  gsize initial_len;
  TFValuePairsResultType result_type;
} TFValuePairsIterState;

static gboolean
tf_value_pairs_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                       gint argc, gchar *argv[],
                       GError **error)
{
  TFValuePairsState *state = (TFValuePairsState *)s;

  if (strcmp(argv[0], "values") == 0)
    state->result_type = TFVP_VALUES;
  else if(strcmp(argv[0], "names") == 0)
    state->result_type = TFVP_NAMES;
  else
    g_assert_not_reached();

  state->vp = value_pairs_new_from_cmdline (parent->cfg, &argc, &argv, NULL, NULL, error);
  return state->vp != NULL;
}

static gboolean
tf_value_pairs_foreach(const gchar *name,
                       LogMessageValueType type,
                       const gchar *value, gsize value_len,
                       gpointer user_data)
{
  TFValuePairsIterState *iter_state = (TFValuePairsIterState *) user_data;

  _append_comma_between_list_elements_if_needed(iter_state->result, iter_state->initial_len);

  switch (iter_state->result_type)
    {
    case TFVP_VALUES:
      str_repr_encode_append(iter_state->result, value, value_len, ",");
      break;
    case TFVP_NAMES:
      str_repr_encode_append(iter_state->result, name, -1, ",");
      break;
    default:
      g_assert_not_reached();
    }

  return FALSE;
}

static void
tf_value_pairs_call(LogTemplateFunction *self, gpointer s,
                    const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  TFValuePairsState *state = (TFValuePairsState *)s;
  TFValuePairsIterState iter_state =
  {
    .result = result,
    .initial_len = result->len,
    .result_type = state->result_type,
  };
  *type = LM_VT_LIST;

  value_pairs_foreach(state->vp,
                      tf_value_pairs_foreach,
                      args->messages[args->num_messages-1],
                      args->options, &iter_state);
}

static void
tf_value_pairs_free_state(gpointer s)
{
  TFValuePairsState *state = (TFValuePairsState *)s;

  value_pairs_unref(state->vp);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFValuePairsState, tf_value_pairs, tf_value_pairs_prepare, NULL, tf_value_pairs_call,
                  tf_value_pairs_free_state, NULL);
