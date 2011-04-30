/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Balint Kovacs <blint@balabit.hu>
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
 */

#include "plugin.h"
#include "templates.h"
#include "filter.h"
#include "filter-expr-parser.h"
#include "cfg.h"
#include "value-pairs.h"

#include <json.h>

static gboolean
tf_json_prepare(LogTemplateFunction *self, LogTemplate *parent,
		gint argc, gchar *argv[],
		gpointer *state, GDestroyNotify *state_destroy,
		GError **error)
{
  ValuePairs *vp;

  vp = value_pairs_new_from_cmdline (parent->cfg, argc, argv, error);
  if (!vp)
    return FALSE;

  *state = vp;
  *state_destroy = (GDestroyNotify) value_pairs_free;

  return TRUE;
}

static gboolean
tf_json_foreach (const gchar *name, const gchar *value, gpointer user_data)
{
  struct json_object *root = (struct json_object *)user_data;
  struct json_object *this;

  this = json_object_new_string ((gchar *) value);
  json_object_object_add (root, (gchar *) name, this);

  return FALSE;
}

static struct json_object *
tf_json_format_message(ValuePairs *vp, LogMessage *msg)
{
  struct json_object *root;

  root = json_object_new_object();
  value_pairs_foreach (vp, tf_json_foreach, msg, 0, root);

  return root;
}

static void
tf_json_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
	     LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
	     gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  gint i;
  ValuePairs *vp = (ValuePairs *)state;

  for (i = 0; i < num_messages; i++)
    {
      LogMessage *msg = messages[i];
      struct json_object *json;

      json = tf_json_format_message(vp, msg);
      g_string_append(result, json_object_to_json_string (json));
      json_object_put(json);
    }
}

static void
tf_json_eval (LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
	      LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
	      gint tz, gint seq_num, const gchar *context_id)
{
  return;
}

TEMPLATE_FUNCTION(tf_json, tf_json_prepare, tf_json_eval, tf_json_call, NULL);

static Plugin builtin_tmpl_func_plugins[] =
  {
    TEMPLATE_FUNCTION_PLUGIN(tf_json, "format_json"),
  };

gboolean
tfjson_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, builtin_tmpl_func_plugins, G_N_ELEMENTS(builtin_tmpl_func_plugins));
  return TRUE;
}
