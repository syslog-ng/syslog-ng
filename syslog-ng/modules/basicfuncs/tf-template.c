/*
 * Copyright (c) 2002-2014 Balabit
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

/*
 * $(template <template-name> ...)
 *
 *   This template function looks up <template-name> in the configuration
 *   and uses that format its result.  The template name can either be a
 *   static or a dynamic reference.
 *
 *     * static: means that we perform lookup at configuration read time
 *
 *     * dynamic: the template name is actually a template (e.g.
 *       ${foobar}), its result are taken as the name of the template and
 *       that is looked up at runtime.
 *
 *   Dynamic references are slower, but more flexible.  syslog-ng uses the
 *   following algorithm to determine which one to use:
 *
 *     * static: if the name of the template can be resolved at compile
 *       time, the binding becomes static.
 *
 *     * dynamic: if static lookup fails and the template name contains at
 *       least one '$' character to indicate that it is actually a template.
 *
 *   In case of dynamic we allow 2nd and further arguments which will be
 *   used as content if the lookup fails.
 *
 *   Examples:
 *      $(template foobar)     -> static binding
 *      $(template ${foobar})  -> dynamic binding
 *      $(template ${foobar} '$DATE $HOST $MSGHDR$MSG\n') -> dynamic binding with fallback
 *
 */
typedef struct _TFTemplateState
{
  TFSimpleFuncState super;
  GlobalConfig *cfg;
  LogTemplate *invoked_template;
} TFTemplateState;

static LogTemplate *
tf_template_lookup_invoked_template(TFTemplateState *state, GlobalConfig *cfg, const gchar *function_name,
                                    GError **error)
{
  return cfg_tree_lookup_template(&cfg->tree, function_name);
}

static const gchar *
tf_template_extract_invoked_template_name_from_args(gint argc, gchar *argv[])
{
  if (argc >= 2 && strcmp(argv[0], "template") == 0)
    return argv[1];
  return NULL;
}

static gboolean
tf_template_statically_bound(TFTemplateState *state)
{
  return state->invoked_template != NULL;
}

static gboolean
tf_template_dynamically_bound(TFTemplateState *state)
{
  return !tf_template_statically_bound(state);
}

static gboolean
tf_template_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                    GError **error)
{
  TFTemplateState *state = (TFTemplateState *) s;
  const gchar *invoked_template_name;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  invoked_template_name = tf_template_extract_invoked_template_name_from_args(argc, argv);
  if (!invoked_template_name)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(template) requires one argument, that specifies the template name to be invoked");
      return FALSE;
    }

  state->invoked_template = tf_template_lookup_invoked_template(state, parent->cfg, invoked_template_name, error);
  if (tf_template_statically_bound(state))
    return TRUE;

  /* compile time lookup failed, let's check if it's a dynamically bound invocation */
  if (strchr(invoked_template_name, '$') == NULL)
    {
      /* our argument is not a template, no chance of being better at
       * runtime, raise as an error */

      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(template) Unknown template function or template \"%s\"",
                  invoked_template_name);
      return FALSE;
    }

  state->cfg = parent->cfg;
  return tf_simple_func_prepare(self, s, parent, argc, argv, error);
}

static void
tf_template_eval(LogTemplateFunction *self, gpointer s, LogTemplateInvokeArgs *args)
{
  TFTemplateState *state = (TFTemplateState *) s;

  if (tf_template_dynamically_bound(state))
    tf_simple_func_eval(self, s, args);
}

static LogTemplate *
tf_template_get_template_to_be_invoked(TFTemplateState *state, const LogTemplateInvokeArgs *args)
{
  LogTemplate *invoked_template;

  if (tf_template_dynamically_bound(state))
    {
      const gchar *template_name = args->argv[0]->str;

      invoked_template = cfg_tree_lookup_template(&state->cfg->tree, template_name);

      msg_trace("$(template) dynamic template lookup result",
                evt_tag_str("template", template_name),
                evt_tag_int("found", invoked_template != NULL));
    }
  else
    {
      invoked_template = log_template_ref(state->invoked_template);
    }
  return invoked_template;
}

static void
tf_template_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFTemplateState *state = (TFTemplateState *) s;
  LogTemplate *invoked_template = NULL;

  invoked_template = tf_template_get_template_to_be_invoked(state, args);
  if (!invoked_template)
    {
      _append_args_with_separator(state->super.argc - 1, (GString **) &args->argv[1], result, ' ');
      return;
    }

  log_template_append_format_with_context(invoked_template, args->messages, args->num_messages, args->opts,
                                          args->tz, args->seq_num, args->context_id, result);
  log_template_unref(invoked_template);
}

static void
tf_template_free_state(gpointer s)
{
  TFTemplateState *state = (TFTemplateState *) s;

  log_template_unref(state->invoked_template);
  tf_simple_func_free_state(s);
}

TEMPLATE_FUNCTION(TFTemplateState, tf_template,
                  tf_template_prepare, tf_template_eval, tf_template_call, tf_template_free_state,
                  NULL);
