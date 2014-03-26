/*
 * Copyright (c) 2011-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Balint Kovacs <blint@balabit.hu>
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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
#include "template/templates.h"
#include "filter.h"
#include "filter-expr-parser.h"
#include "cfg.h"
#include "value-pairs.h"
#include "vptransform.h"
#include "syslog-ng.h"

static gboolean
tf_json_prepare(LogTemplateFunction *self, LogTemplate *parent,
                gint argc, gchar *argv[],
                gpointer *state, GDestroyNotify *state_destroy,
                GError **error)
{
  ValuePairs *vp;
  ValuePairsTransformSet *vpts;

  vp = value_pairs_new_from_cmdline (parent->cfg, argc, argv, error);
  if (!vp)
    return FALSE;

  *state = vp;
  *state_destroy = (GDestroyNotify) value_pairs_free;

  /* Always replace a leading dot with an underscore. */
  vpts = value_pairs_transform_set_new(".*");
  value_pairs_transform_set_add_func(vpts, value_pairs_new_transform_replace_prefix(".", "_"));
  value_pairs_add_transforms(vp, vpts);

  return TRUE;
}


typedef struct
{
  gboolean need_comma;
  GString *buffer;
  LogTemplateOptions *template_options;
} json_state_t;

static inline void
g_string_append_escaped(GString *dest, const char *str)
{
  /* Assumes ASCII!  Keep in sync with the switch! */
  static const unsigned char json_exceptions[UCHAR_MAX + 1] =
    {
      [0x01] = 1, [0x02] = 1, [0x03] = 1, [0x04] = 1, [0x05] = 1, [0x06] = 1,
      [0x07] = 1, [0x08] = 1, [0x09] = 1, [0x0a] = 1, [0x0b] = 1, [0x0c] = 1,
      [0x0d] = 1, [0x0e] = 1, [0x0f] = 1, [0x10] = 1, [0x11] = 1, [0x12] = 1,
      [0x13] = 1, [0x14] = 1, [0x15] = 1, [0x16] = 1, [0x17] = 1, [0x18] = 1,
      [0x19] = 1, [0x1a] = 1, [0x1b] = 1, [0x1c] = 1, [0x1d] = 1, [0x1e] = 1,
      [0x1f] = 1, ['\\'] = 1, ['"'] = 1
    };

  const unsigned char *p;

  p = (unsigned char *)str;

  while (*p)
    {
      if (json_exceptions[*p] == 0)
        g_string_append_c(dest, *p);
      else
        {
          /* Keep in sync with json_exceptions! */
          switch (*p)
            {
            case '\b':
              g_string_append(dest, "\\b");
              break;
            case '\n':
              g_string_append(dest, "\\n");
              break;
            case '\r':
              g_string_append(dest, "\\r");
              break;
            case '\t':
              g_string_append(dest, "\\t");
              break;
            case '\\':
              g_string_append(dest, "\\\\");
              break;
            case '"':
              g_string_append(dest, "\\\"");
              break;
            default:
              {
                static const char json_hex_chars[16] = "0123456789abcdef";

                g_string_append(dest, "\\u00");
                g_string_append_c(dest, json_hex_chars[(*p) >> 4]);
                g_string_append_c(dest, json_hex_chars[(*p) & 0xf]);
                break;
              }
            }
        }
      p++;
    }
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
      g_string_append_escaped(state->buffer, name);
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

static gboolean
tf_json_append_value(const gchar *name, const gchar *value,
                     json_state_t *state, gboolean quoted)
{
  if (state->need_comma)
    g_string_append_c(state->buffer, ',');

  g_string_append_c(state->buffer, '"');
  g_string_append_escaped(state->buffer, name);

  if (quoted)
    g_string_append(state->buffer, "\":\"");
  else
    g_string_append(state->buffer, "\":");

  g_string_append_escaped(state->buffer, value);

  if (quoted)
    g_string_append_c(state->buffer, '"');

  return TRUE;
}

static gboolean
tf_json_value(const gchar *name, const gchar *prefix,
              TypeHint type, const gchar *value,
              gpointer *prefix_data, gpointer user_data)
{
  json_state_t *state = (json_state_t *)user_data;
  gint on_error = state->template_options->on_error;

  switch (type)
    {
    case TYPE_HINT_STRING:
    case TYPE_HINT_DATETIME:
    default:
      tf_json_append_value(name, value, state, TRUE);
      break;
    case TYPE_HINT_LITERAL:
      tf_json_append_value(name, value, state, FALSE);
      break;
    case TYPE_HINT_INT32:
    case TYPE_HINT_INT64:
    case TYPE_HINT_BOOLEAN:
      {
        gint32 i32;
        gint64 i64;
        gboolean b;
        gboolean r = FALSE, fail = FALSE;
        const gchar *v = value;

        if (type == TYPE_HINT_INT32 &&
            (fail = !type_cast_to_int32(value, &i32 , NULL)) == TRUE)
          r = type_cast_drop_helper(on_error, value, "int32");
        else if (type == TYPE_HINT_INT64 &&
            (fail = !type_cast_to_int64(value, &i64 , NULL)) == TRUE)
          r = type_cast_drop_helper(on_error, value, "int64");
        else if (type == TYPE_HINT_BOOLEAN)
          {
            if ((fail = !type_cast_to_boolean(value, &b , NULL)) == TRUE)
              r = type_cast_drop_helper(on_error, value, "boolean");
            else
              v = b ? "true" : "false";
          }

        if (fail &&
            !(on_error & ON_ERROR_FALLBACK_TO_STRING))
          return r;

        tf_json_append_value(name, v, state, fail);
        break;
      }
    }

  state->need_comma = TRUE;

  return FALSE;
}

static gboolean
tf_json_append(GString *result, ValuePairs *vp, LogMessage *msg,
               LogTemplateOptions *template_options, gint time_zone_mode, gint seq_num)
{
  json_state_t state;

  state.need_comma = FALSE;
  state.buffer = result;
  state.template_options = template_options;

  return value_pairs_walk(vp,
                          tf_json_obj_start, tf_json_value, tf_json_obj_end,
                          msg, seq_num, time_zone_mode,
                          template_options,
                          &state);
}

static void
tf_json_call(LogTemplateFunction *self, gpointer state, GPtrArray *arg_bufs,
             LogMessage **messages, gint num_messages, LogTemplateOptions *opts,
             gint tz, gint seq_num, const gchar *context_id, GString *result)
{
  gint i;
  gboolean r = TRUE;
  gsize orig_size = result->len;
  ValuePairs *vp = (ValuePairs *)state;

  for (i = 0; i < num_messages; i++)
    r &= tf_json_append(result, vp, messages[i], opts, tz, seq_num);

  if (!r && (opts->on_error & ON_ERROR_DROP_MESSAGE))
    g_string_set_size(result, orig_size);
}

TEMPLATE_FUNCTION(tf_json, tf_json_prepare, NULL, tf_json_call, NULL);
