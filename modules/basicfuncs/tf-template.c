/*
 * Copyright (c) 2002-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2014 Balázs Scheidler
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
 *
 */

#include "cfg.h"

typedef struct _TFTemplateState
{
  LogTemplate *invoked_template;
} TFTemplateState;

static gboolean
tf_template_lookup_invoked_template(TFTemplateState *state, GlobalConfig *cfg, const gchar *function_name, GError **error)
{
  state->invoked_template = cfg_tree_lookup_template(&cfg->tree, function_name);
  if (!state->invoked_template)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Unknown template function or template \"%s\"", function_name);
      return FALSE;
    }
  return TRUE;
}

static const gchar *
tf_template_extract_invoked_template_name_from_args(gint argc, gchar *argv[])
{
  if (argc == 2 && strcmp(argv[0], "template") == 0)
    return argv[1];
  return NULL;
}

static gboolean
tf_template_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFTemplateState *state = (TFTemplateState *) s;
  const gchar *invoked_template_name;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  invoked_template_name = tf_template_extract_invoked_template_name_from_args(argc, argv);
  if (!invoked_template_name)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(template) requires one argument, that specifies the template name to be invoked");
      return FALSE;
    }

  return tf_template_lookup_invoked_template(state, parent->cfg, invoked_template_name, error);
}

static void
tf_template_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFTemplateState *state = (TFTemplateState *) s;

  log_template_append_format_with_context(state->invoked_template, args->messages, args->num_messages, args->opts, args->tz, args->seq_num, args->context_id, result);
}

static void
tf_template_free_state(gpointer s)
{
  TFTemplateState *state = (TFTemplateState *) s;

  log_template_unref(state->invoked_template);
}

TEMPLATE_FUNCTION(TFTemplateState, tf_template, tf_template_prepare, NULL, tf_template_call, tf_template_free_state, NULL);
