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

#include "config.h"

#ifdef HAVE_JSON_C
#include <json.h>
#endif

#ifdef HAVE_JSON_GLIB
#include <json-glib/json-glib.h>
#endif

typedef struct _TFJsonState
{
  TFSimpleFuncState super;
  ValuePairs *vp;
} TFJsonState;

static gboolean
tf_json_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
		gint argc, gchar *argv[],
		GError **error)
{
  TFJsonState *state = (TFJsonState *)s;

  state->vp = value_pairs_new_from_cmdline (parent->cfg, argc, argv, error);
  if (!state->vp)
    return FALSE;

  return TRUE;
}

#if HAVE_JSON_C
static gboolean
tf_json_foreach (const gchar *name, const gchar *value, gpointer user_data)
{
  struct json_object *root = (struct json_object *)user_data;
  struct json_object *this;

  this = json_object_new_string ((gchar *) value);
  json_object_object_add (root, (gchar *) name, this);

  return FALSE;
}

static void
tf_json_append(GString *result, ValuePairs *vp, LogMessage *msg)
{
  struct json_object *json;

  json = json_object_new_object();

  value_pairs_foreach(vp, tf_json_foreach, msg, 0, json);

  g_string_append(result, json_object_to_json_string (json));
  json_object_put(json);
}
#endif

#if HAVE_JSON_GLIB
static gboolean
tf_json_foreach (const gchar *name, const gchar *value, gpointer user_data)
{
  JsonBuilder *builder = (JsonBuilder *)user_data;

  json_builder_set_member_name(builder, name);
  json_builder_add_string_value(builder, value);

  return FALSE;
}

static void
tf_json_append(GString *result, ValuePairs *vp, LogMessage *msg)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  gchar *str;

  builder = json_builder_new();
  json_builder_begin_object(builder);

  value_pairs_foreach(vp, tf_json_foreach, msg, 0, builder);

  json_builder_end_object(builder);

  gen = json_generator_new();
  json_generator_set_root(gen, json_builder_get_root(builder));
  str = json_generator_to_data(gen, NULL);

  g_object_unref(gen);
  g_object_unref(builder);

  g_string_append(result, str);
  g_free(str);
}
#endif

static void
tf_json_call(LogTemplateFunction *self, gpointer s,
	     const LogTemplateInvokeArgs *args, GString *result)
{
  TFJsonState *state = (TFJsonState *)s;
  gint i;

  for (i = 0; i < args->num_messages; i++)
    tf_json_append(result, state->vp, args->messages[i]);
}

static void
tf_json_free_state(gpointer s)
{
  TFJsonState *state = (TFJsonState *)s;

  if (state->vp)
    value_pairs_free(state->vp);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFJsonState, tf_json, tf_json_prepare, NULL, tf_json_call,
		  tf_json_free_state, NULL);

static Plugin builtin_tmpl_func_plugins[] =
  {
    TEMPLATE_FUNCTION_PLUGIN(tf_json, "format_json"),
  };

gboolean
tfjson_module_init(GlobalConfig *cfg, CfgArgs *args)
{
#ifdef HAVE_JSON_GLIB
  g_type_init ();
#endif

  plugin_register(cfg, builtin_tmpl_func_plugins, G_N_ELEMENTS(builtin_tmpl_func_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "tfjson",
  .version = VERSION,
  .description = "The tfjson module provides a JSON formatting template function for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = builtin_tmpl_func_plugins,
  .plugins_len = G_N_ELEMENTS(builtin_tmpl_func_plugins),
};
