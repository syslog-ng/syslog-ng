/*
 * Copyright (c) 2015 Balabit
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

#include "format-welf.h"
#include "utf8utils.h"
#include "value-pairs/cmdline.h"

typedef struct _TFWelfState
{
  TFSimpleFuncState super;
  ValuePairs *vp;
} TFWelfState;

typedef struct _TFWelfIterState
{
  GString *result;
  gboolean initial_kv_pair_printed;
} TFWelfIterState;

static gboolean
tf_format_welf_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                       gint argc, gchar *argv[],
                       GError **error)
{
  TFWelfState *state = (TFWelfState *) s;

  state->vp = value_pairs_new_from_cmdline (parent->cfg, argc, argv, error);
  if (!state->vp)
    return FALSE;

  return TRUE;
}

static gboolean
tf_format_welf_foreach(const gchar *name, TypeHint type, const gchar *value,
                       gsize value_len, gpointer user_data)
{
  TFWelfIterState *iter_state = (TFWelfIterState *) user_data;
  GString *result = iter_state->result;

  if (iter_state->initial_kv_pair_printed)
    g_string_append(result, " ");
  else
    iter_state->initial_kv_pair_printed = TRUE;

  g_string_append(result, name);
  g_string_append_c(result, '=');
  if (memchr(value, ' ', value_len) == NULL)
    append_unsafe_utf8_as_escaped_binary(result, value, value_len, NULL);
  else
    {
      g_string_append_c(result, '"');
      append_unsafe_utf8_as_escaped_binary(result, value, value_len, "\"");
      g_string_append_c(result, '"');
    }

  return FALSE;
}

static gint
tf_format_welf_strcmp(gconstpointer a, gconstpointer b)
{
  gchar *sa = (gchar *)a, *sb = (gchar *)b;
  if (strcmp (sa, "id") == 0)
    return -1;
  return strcmp(sa, sb);
}

static void
tf_format_welf_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFWelfState *state = (TFWelfState *) s;
  TFWelfIterState iter_state =
  {
    .result = result,
    .initial_kv_pair_printed = FALSE
  };
  gint i;

  for (i = 0; i < args->num_messages; i++)
    {
      value_pairs_foreach_sorted(state->vp,
                                 tf_format_welf_foreach, (GCompareFunc) tf_format_welf_strcmp,
                                 args->messages[i], 0, args->tz, args->opts, &iter_state);
    }

}

static void
tf_format_welf_free_state(gpointer s)
{
  TFWelfState *state = (TFWelfState *) s;

  value_pairs_unref(state->vp);
  tf_simple_func_free_state(s);
}

TEMPLATE_FUNCTION(TFWelfState, tf_format_welf, tf_format_welf_prepare, NULL, tf_format_welf_call,
                  tf_format_welf_free_state, NULL);
