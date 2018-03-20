/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa <tusa@balabit.hu>
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
 */

#include "graphite-output.h"
#include "template/templates.h"
#include "logmsg/logmsg.h"
#include "value-pairs/value-pairs.h"
#include "value-pairs/cmdline.h"

typedef struct _TFGraphiteState
{
  ValuePairs *vp;
  LogTemplate *timestamp_template;
} TFGraphiteState;

typedef struct _TFGraphiteArgumentsUserData
{
  TFGraphiteState *state;
  GlobalConfig *cfg;
} TFGraphiteArgumentsUserData;

static gboolean
tf_graphite_set_timestamp(const gchar *option_name, const gchar *value,
                          gpointer data, GError **error)
{
  TFGraphiteArgumentsUserData *args = (TFGraphiteArgumentsUserData *) data;

  args->state->timestamp_template = log_template_new(args->cfg, NULL);
  log_template_compile(args->state->timestamp_template, value, NULL);
  return TRUE;
};

static gboolean
tf_graphite_parse_command_line_arguments(TFGraphiteState *self, gint *argc, gchar ***argv, LogTemplate *parent)
{
  GOptionContext *ctx;
  GOptionGroup *og;
  TFGraphiteArgumentsUserData userdata;
  gboolean success;
  GError *error = NULL;

  GOptionEntry graphite_options[] =
  {
    { "timestamp", 't', 0, G_OPTION_ARG_CALLBACK, tf_graphite_set_timestamp, NULL, NULL },
    { NULL },
  };

  userdata.state = self;
  userdata.cfg = parent->cfg;

  ctx = g_option_context_new ("graphite-options");
  og = g_option_group_new (NULL, NULL, NULL, &userdata, NULL);
  g_option_group_add_entries(og, graphite_options);
  g_option_context_set_main_group(ctx, og);
  g_option_context_set_ignore_unknown_options(ctx, TRUE);

  success = g_option_context_parse (ctx, argc, argv, &error);
  g_option_context_free (ctx);

  return success;
}

static gboolean
tf_graphite_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                    gint argc, gchar *argv[],
                    GError **error)
{
  TFGraphiteState *state = (TFGraphiteState *)s;
  ValuePairsTransformSet *vpts;

  if (!tf_graphite_parse_command_line_arguments(state, &argc, &argv, parent))
    return FALSE;

  if (!state->timestamp_template)
    {
      state->timestamp_template = log_template_new(parent->cfg, NULL);
      log_template_compile(state->timestamp_template, "$R_UNIXTIME", NULL);
    }

  state->vp = value_pairs_new_from_cmdline (parent->cfg, &argc, &argv, FALSE, error);
  if (!state->vp)
    return FALSE;

  /* Always replace a leading dot with an underscore. */
  vpts = value_pairs_transform_set_new(".*");
  value_pairs_transform_set_add_func(vpts, value_pairs_new_transform_replace_prefix(".", "_"));
  value_pairs_add_transforms(state->vp, vpts);

  return TRUE;
}

typedef struct _TFGraphiteForeachUserData
{
  GString *formatted_unixtime;
  GString *result;
} TFGraphiteForeachUserData;

/* TODO escape '\0' when passing down the value */
static gboolean
tf_graphite_foreach_func(const gchar *name, TypeHint type, const gchar *value,
                         gsize value_len, gpointer user_data)
{
  TFGraphiteForeachUserData *data = (TFGraphiteForeachUserData *) user_data;

  g_string_append(data->result, name);
  g_string_append_c(data->result,' ');
  g_string_append(data->result, value);
  g_string_append_c(data->result,' ');
  g_string_append(data->result, data->formatted_unixtime->str);
  g_string_append_c(data->result,'\n');

  return FALSE;
}

static gboolean
tf_graphite_format(GString *result, ValuePairs *vp, LogMessage *msg, const LogTemplateOptions *template_options,
                   LogTemplate *timestamp_template, gint time_zone_mode)
{
  TFGraphiteForeachUserData userdata;
  gboolean return_value;

  userdata.result = result;
  userdata.formatted_unixtime = g_string_new("");
  log_template_format(timestamp_template, msg, NULL, 0, 0, NULL, userdata.formatted_unixtime);

  return_value = value_pairs_foreach(vp, tf_graphite_foreach_func, msg, 0, time_zone_mode, template_options, &userdata);

  g_string_free(userdata.formatted_unixtime, FALSE);
  return return_value;
}

static void
tf_graphite_call(LogTemplateFunction *self, gpointer s,
                 const LogTemplateInvokeArgs *args, GString *result)
{
  TFGraphiteState *state = (TFGraphiteState *)s;
  gint i;
  gboolean r = TRUE;
  gsize orig_size = result->len;

  for (i = 0; i < args->num_messages; i++)
    r &= tf_graphite_format(result, state->vp, args->messages[i], args->opts, state->timestamp_template, args->tz);

  if (!r && (args->opts->on_error & ON_ERROR_DROP_MESSAGE))
    g_string_set_size(result, orig_size);
}

static void
tf_graphite_free_state(gpointer s)
{
  TFGraphiteState *state = (TFGraphiteState *)s;

  value_pairs_unref(state->vp);
  log_template_unref(state->timestamp_template);
}

TEMPLATE_FUNCTION(TFGraphiteState, tf_graphite, tf_graphite_prepare, NULL, tf_graphite_call,
                  tf_graphite_free_state, NULL);

