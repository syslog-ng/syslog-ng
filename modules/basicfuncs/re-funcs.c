/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2014 Viktor Tusa <tusavik@gmail.com>
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

#include "compat/pcre.h"

/*
 * $(re-extract <regexp> $arg1 $arg2 ...)
 */
typedef struct _TFRegexpState
{
  TFSimpleFuncState super;
  pcre *pattern;
  pcre_extra *extra;
} TFRegexpState;

static gboolean
tf_re_extract_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                    GError **error)
{
  TFRegexpState *state = (TFRegexpState *) s;
  gint rc;
  const gchar *errptr;
  gint erroffset;

  if (argc != 3)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(re-extract): requires two positional arguments, <regexp> and <string>");
      return FALSE;
    }

  /* compile the regexp */
  state->pattern = pcre_compile2(argv[1], 0, &rc, &errptr, &erroffset, NULL);
  if (!state->pattern)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(re-extract): failed to compile PCRE expression >>>%s<<< `%s' at character %d",
                  argv[1], errptr, erroffset);
      return FALSE;
    }

  state->extra = pcre_study(state->pattern, 0, &errptr);
  if (errptr != NULL)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, 0, "$(re-extract): failed to optimize regular expression >>>%s<<< `%s'",
                  argv[1], errptr);
      return FALSE;
    }

  memmove(&argv[1], &argv[2], sizeof(argv[0]) * (argc - 2));
  return tf_simple_func_prepare(self, state, parent, argc-1, argv, error);
}

static void
tf_re_extract_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result,
                   LogMessageValueType *type)
{
  TFRegexpState *state = (TFRegexpState *) s;

  *type = LM_VT_STRING;

  gint *matches;
  gsize matches_size;
  gint num_matches;
  gint rc;

  if (pcre_fullinfo(state->pattern, state->extra, PCRE_INFO_CAPTURECOUNT, &num_matches) < 0)
    g_assert_not_reached();
  if (num_matches > LOGMSG_MAX_MATCHES)
    num_matches = LOGMSG_MAX_MATCHES;

  matches_size = 3 * (num_matches + 1);
  matches = g_alloca(matches_size * sizeof(gint));

  const gchar *value = args->argv[0]->str;
  gsize value_len = args->argv[0]->len;

  rc = pcre_exec(state->pattern, state->extra,
                 value, value_len, 0, 0, matches, matches_size);
  if (rc > 0)
    {
      gint i = 1;
      gint begin_index = matches[2 * i];
      gint end_index = matches[2 * i + 1];

      if (!(begin_index < 0 || end_index < 0))
        g_string_append_len(result, &value[begin_index], end_index - begin_index);
    }
}

static void
tf_re_extract_free_state(gpointer s)
{
  TFRegexpState *state = (TFRegexpState *) s;

  pcre_free_study(state->extra);
  pcre_free(state->pattern);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFRegexpState, tf_re_extract, tf_re_extract_prepare, tf_simple_func_eval, tf_re_extract_call,
                  tf_re_extract_free_state, NULL);
