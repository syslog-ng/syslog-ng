/*
 * Copyright (c) 2011-2014 Balabit
 * Copyright (c) 2011 Balint Kovacs <blint@balabit.hu>
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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

#include "plugin.h"
#include "template/simple-function.h"
#include "filter/filter-expr.h"
#include "filter/filter-expr-parser.h"
#include "cfg.h"
#include "value-pairs/cmdline.h"
#include "syslog-ng.h"
#include "utf8utils.h"
#include "scanner/list-scanner/list-scanner.h"
#include "scratch-buffers.h"

typedef struct _TFJsonState
{
  TFSimpleFuncState super;
  ValuePairs *vp;
  gchar key_delimiter;
} TFJsonState;

static gboolean
_parse_key_delimiter(const gchar *option_name,
                     const gchar *value,
                     gpointer data,
                     GError **error)
{
  TFJsonState *state = (TFJsonState *) data;

  if (strlen(value) > 1 || strlen(value) == 0)
    {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                  "$(format-json) --key-delimiter only accepts a single character, found: '%s'", value);
      return FALSE;
    }
  /* take off the first character */
  state->key_delimiter = value[0];
  return TRUE;
}

static gboolean
tf_json_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                gint argc, gchar *argv[],
                GError **error)
{
  TFJsonState *state = (TFJsonState *)s;
  ValuePairsTransformSet *vpts;
  gboolean transform_initial_dot = TRUE;

  state->key_delimiter = '.';
  GOptionEntry format_json_options[] =
  {
    { "leave-initial-dot", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &transform_initial_dot, NULL, NULL },
    { "key-delimiter", 0, 0, G_OPTION_ARG_CALLBACK, _parse_key_delimiter, NULL, NULL },
    { NULL },
  };

  GOptionGroup *og = g_option_group_new("format-json", "", "", state, NULL);
  g_option_group_add_entries(og, format_json_options);

  state->vp = value_pairs_new_from_cmdline(parent->cfg, &argc, &argv, og, error);
  if (!state->vp)
    return FALSE;

  if (transform_initial_dot && state->key_delimiter == '.')
    {
      /* Always replace a leading dot with an underscore, unless something
       * other than a dot is specified as key-delimiter.  */

      vpts = value_pairs_transform_set_new(".*");
      value_pairs_transform_set_add_func(vpts, value_pairs_new_transform_replace_prefix(".", "_"));
      value_pairs_add_transforms(state->vp, vpts);
    }

  if (cfg_is_config_version_older(parent->cfg, VERSION_VALUE_4_0) &&
      !value_pairs_is_cast_to_strings_explicit(state->vp))
    {
      if (cfg_is_typing_feature_enabled(parent->cfg))
        {
          msg_warning("WARNING: $(format-json) starts using type information "
                      "associated with name-value pairs in " VERSION_4_0
                      ". This can possibly cause fields in the formatted JSON "
                      "document to change types if no explicit type hint is "
                      "specified. This change will cause the type in the output "
                      "document match the original type that was parsed "
                      "using json-parser(), add --no-cast argument "
                      "to $(format-json) to keep the old behavior");
        }
      value_pairs_set_cast_to_strings(state->vp, TRUE);
    }

  return TRUE;
}

typedef struct
{
  gboolean need_comma;
  GString *buffer;
  const LogTemplateOptions *template_options;
} json_state_t;

static inline void
tf_json_append_escaped(GString *dest, const gchar *str, gssize str_len)
{
  append_unsafe_utf8_as_escaped_text(dest, str, str_len, "\"");
}

static gboolean
tf_json_obj_start(const gchar *name,
                  const gchar *prefix, gpointer *prefix_data,
                  const gchar *prev, gpointer *prev_data,
                  gpointer user_data)
{
  json_state_t *state = (json_state_t *)user_data;

  if (state->need_comma)
    g_string_append_c(state->buffer, ',');

  if (name)
    {
      g_string_append_c(state->buffer, '"');
      tf_json_append_escaped(state->buffer, name, -1);
      g_string_append(state->buffer, "\":{");
    }
  else
    g_string_append_c(state->buffer, '{');

  state->need_comma = FALSE;

  return FALSE;
}

static gboolean
tf_json_obj_end(const gchar *name,
                const gchar *prefix, gpointer *prefix_data,
                const gchar *prev, gpointer *prev_data,
                gpointer user_data)
{
  json_state_t *state = (json_state_t *)user_data;

  g_string_append_c(state->buffer, '}');

  state->need_comma = TRUE;

  return FALSE;
}

static void
tf_json_append_key(const gchar *name, json_state_t *state)
{
  if (state->need_comma)
    g_string_append_c(state->buffer, ',');

  g_string_append_c(state->buffer, '"');
  tf_json_append_escaped(state->buffer, name, -1);
  g_string_append_c(state->buffer, '"');

}

static void
tf_json_append_value(const gchar *name, const gchar *value, gsize value_len,
                     json_state_t *state, gboolean quoted)
{
  tf_json_append_key(name, state);

  if (quoted)
    g_string_append(state->buffer, ":\"");
  else
    g_string_append_c(state->buffer, ':');

  tf_json_append_escaped(state->buffer, value, value_len);

  if (quoted)
    g_string_append_c(state->buffer, '"');
}

static void
tf_json_append_literal(const gchar *name, const gchar *value, gsize value_len,
                       json_state_t *state)
{
  tf_json_append_key(name, state);

  g_string_append_c(state->buffer, ':');
  g_string_append_len(state->buffer, value, value_len);
}

static void
tf_json_append_list(const gchar *name, const gchar *value, gsize value_len,
                    json_state_t *state)
{
  tf_json_append_key(name, state);

  g_string_append_c(state->buffer, ':');
  g_string_append_c(state->buffer, '[');

  ListScanner scanner;
  gboolean first = TRUE;

  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, value, value_len);
  while (list_scanner_scan_next(&scanner))
    {
      if (!first)
        g_string_append_c(state->buffer, ',');
      else
        first = FALSE;
      g_string_append_c(state->buffer, '"');
      tf_json_append_escaped(state->buffer, list_scanner_get_current_value(&scanner), -1);
      g_string_append_c(state->buffer, '"');
    }

  list_scanner_deinit(&scanner);
  g_string_append_c(state->buffer, ']');
}

static gboolean
tf_json_append_with_type_hint(const gchar *name, LogMessageValueType type, json_state_t *state, const gchar *value,
                              const gssize value_len, const gboolean on_error)
{
  switch (type)
    {
    case LM_VT_STRING:
    case LM_VT_DATETIME:
    default:
      tf_json_append_value(name, value, value_len, state, TRUE);
      break;
    case LM_VT_JSON:
      tf_json_append_literal(name, value, value_len, state);
      break;
    case LM_VT_LIST:
      tf_json_append_list(name, value, value_len, state);
      break;
    case LM_VT_INT32:
    {
      gint32 i32;
      const gchar *v = value;
      gsize v_len = value_len;

      if (!type_cast_to_int32(value, &i32, NULL))
        {
          if ((on_error & ON_ERROR_FALLBACK_TO_STRING))
            tf_json_append_value(name, v, v_len, state, TRUE);
          else
            return type_cast_drop_helper(on_error, value, "int32");
        }
      else
        {
          tf_json_append_value(name, v, v_len, state, FALSE);
        }
      break;
    }
    case LM_VT_INT64:
    {
      gint64 i64;
      const gchar *v = value;
      gsize v_len = value_len;

      if (!type_cast_to_int64(value, &i64, NULL))
        {
          if ((on_error & ON_ERROR_FALLBACK_TO_STRING))
            tf_json_append_value(name, v, v_len, state, TRUE);
          else
            return type_cast_drop_helper(on_error, value, "int64");
        }
      else
        {
          tf_json_append_value(name, v, v_len, state, FALSE);
        }
      break;
    }
    case LM_VT_DOUBLE:
    {
      gdouble d;
      const gchar *v = value;
      gsize v_len = value_len;

      if (!type_cast_to_double(value, &d, NULL))
        {
          if ((on_error & ON_ERROR_FALLBACK_TO_STRING))
            tf_json_append_value(name, v, v_len, state, TRUE);
          else
            return type_cast_drop_helper(on_error, value, "double");
        }
      else
        {
          tf_json_append_value(name, v, v_len, state, FALSE);
        }
      break;
    }
    case LM_VT_BOOLEAN:
    {
      gboolean b;
      const gchar *v = value;
      gsize v_len = value_len;

      if (!type_cast_to_boolean(value, &b, NULL))
        {
          if (!(on_error & ON_ERROR_FALLBACK_TO_STRING))
            return type_cast_drop_helper(on_error, value, "boolean");
          tf_json_append_value(name, v, v_len, state, TRUE);
        }
      else
        {
          v = b ? "true" : "false";
          v_len = -1;
          tf_json_append_value(name, v, v_len, state, FALSE);
        }
      break;
    }
    case LM_VT_NULL:
    {
      tf_json_append_value(name, "null", -1, state, FALSE);
      break;
    }
    }
  return FALSE;
}

static gboolean
tf_json_value(const gchar *name, const gchar *prefix,
              LogMessageValueType type, const gchar *value, gsize value_len,
              gpointer *prefix_data, gpointer user_data)
{
  json_state_t *state = (json_state_t *)user_data;

  gboolean result = tf_json_append_with_type_hint(name, type, state, value, value_len, state->template_options->on_error);

  state->need_comma = TRUE;

  return result;
}

static gboolean
tf_json_append(TFJsonState *state, GString *result, LogMessage *msg, LogTemplateEvalOptions *options)
{
  json_state_t invocation_state;

  invocation_state.need_comma = FALSE;
  invocation_state.buffer = result;
  invocation_state.template_options = options->opts;

  return value_pairs_walk(state->vp,
                          tf_json_obj_start, tf_json_value, tf_json_obj_end,
                          msg, options, state->key_delimiter, &invocation_state);
}

static void
tf_json_call(LogTemplateFunction *self, gpointer s,
             const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  TFJsonState *state = (TFJsonState *)s;
  gsize orig_size = result->len;

  *type = LM_VT_JSON;
  for (gint i = 0; i < args->num_messages; i++)
    {
      gboolean r = tf_json_append(state, result, args->messages[i], args->options);
      if (!r && (args->options->opts->on_error & ON_ERROR_DROP_MESSAGE))
        {
          g_string_set_size(result, orig_size);
          return;
        }
    }

}

static gboolean
tf_flat_json_value(const gchar *name,
                   LogMessageValueType type,
                   const gchar *value, gsize value_len,
                   gpointer user_data)
{
  json_state_t *state = (json_state_t *) user_data;

  gboolean result = tf_json_append_with_type_hint(name, type, state, value, value_len,
                                                  state->template_options->on_error);
  state->need_comma = TRUE;
  return result;
}

static gint
tf_flat_value_pairs_sort(const gchar *s1, const gchar *s2)
{
  return strcmp(s2, s1);
}

static gboolean
tf_flat_json_append(TFJsonState *state, GString *result, LogMessage *msg, LogTemplateEvalOptions *options)
{
  json_state_t invocation_state;

  invocation_state.need_comma = FALSE;
  invocation_state.buffer = result;
  invocation_state.template_options = options->opts;

  g_string_append_c(invocation_state.buffer, '{');

  gboolean success = value_pairs_foreach_sorted(state->vp,
                                                tf_flat_json_value,
                                                (GCompareFunc) tf_flat_value_pairs_sort, msg, options,
                                                &invocation_state);

  g_string_append_c(invocation_state.buffer, '}');

  return success;
}

static void
tf_flat_json_call(LogTemplateFunction *self, gpointer s,
                  const LogTemplateInvokeArgs *args, GString *result, LogMessageValueType *type)
{
  TFJsonState *state = (TFJsonState *)s;
  gsize orig_size = result->len;

  *type = LM_VT_JSON;
  for (gint i = 0; i < args->num_messages; i++)
    {
      gboolean r = tf_flat_json_append(state, result, args->messages[i], args->options);
      if (!r && (args->options->opts->on_error & ON_ERROR_DROP_MESSAGE))
        {
          g_string_set_size(result, orig_size);
          return;
        }
    }

}

static void
tf_json_free_state(gpointer s)
{
  TFJsonState *state = (TFJsonState *)s;

  value_pairs_unref(state->vp);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFJsonState, tf_json, tf_json_prepare, NULL, tf_json_call,
                  tf_json_free_state, NULL);

TEMPLATE_FUNCTION(TFJsonState, tf_flat_json, tf_json_prepare, NULL, tf_flat_json_call,
                  tf_json_free_state, NULL);
