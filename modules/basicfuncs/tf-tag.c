/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

typedef struct _TFTagState
{
  LogTagId tag_id;
  GString *value_if_set;
  GString *value_if_unset;
  gboolean value_is_boolean;
} TFTagState;

static gboolean
tf_tag_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
               GError **error)
{
  TFTagState *state = (TFTagState *) s;

  if (argc < 2 || argc > 4)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "Wrong number of arguments. Example: $(tag tag-name value-if-set value-if-unset).\n");
      return FALSE;
    }

  state->tag_id = log_tags_get_by_name(argv[1]);
  state->value_if_set = g_string_new(argc >= 3 ? argv[2] : "1");
  state->value_if_unset = g_string_new(argc >= 4 ? argv[3] : "0");
  state->value_is_boolean = argc < 3;
  return TRUE;
}


static void
tf_tag_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result,
            LogMessageValueType *type)
{
  TFTagState *state = (TFTagState *) s;
  LogMessage *msg = args->messages[0];

  *type = state->value_is_boolean ? LM_VT_BOOLEAN : LM_VT_STRING;
  if (log_msg_is_tag_by_id(msg, state->tag_id))
    g_string_append_len(result, state->value_if_set->str, state->value_if_set->len);
  else
    g_string_append_len(result, state->value_if_unset->str, state->value_if_unset->len);
}

static void
tf_tag_free_state(gpointer s)
{
  TFTagState *state = (TFTagState *) s;

  g_string_free(state->value_if_set, TRUE);
  g_string_free(state->value_if_unset, TRUE);
}

TEMPLATE_FUNCTION(TFTagState, tf_tag, tf_tag_prepare, NULL, tf_tag_call, tf_tag_free_state, NULL);

typedef struct _TFTagsHeadState
{
  LogTagId *tags;
  gint tags_count;
} TFTagsHeadState;

static gboolean
tf_tags_head_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                     gint argc, gchar *argv[],
                     GError **error)
{
  TFTagsHeadState *state = (TFTagsHeadState *) s;

  if (argc < 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "At least one tag is required. Example: $(tags-head tag1 tag2 ... ).\n");
      return FALSE;
    }

  GArray *l = g_array_new(FALSE, TRUE, sizeof(LogTagId));
  for (gint i = 1; i < argc; i++)
    {
      LogTagId tag_id = log_tags_get_by_name(argv[i]);
      g_array_append_val(l, tag_id);
    }
  state->tags_count = l->len;
  state->tags = (LogTagId *) g_array_free(l, FALSE);
  return TRUE;
}


static void
tf_tags_head_call(LogTemplateFunction *self, gpointer s,
                  const LogTemplateInvokeArgs *args,
                  GString *result, LogMessageValueType *type)
{
  TFTagsHeadState *state = (TFTagsHeadState *) s;
  LogMessage *msg = args->messages[0];

  for (gint i = 0; i < state->tags_count; i++)
    {
      LogTagId id = state->tags[i];
      if (log_msg_is_tag_by_id(msg, id))
        {
          g_string_append(result, log_tags_get_by_id(id));
          *type = LM_VT_STRING;
          return;
        }
    }
  *type = LM_VT_NULL;
}

static void
tf_tags_head_free_state(gpointer s)
{
  TFTagsHeadState *state = (TFTagsHeadState *) s;
  g_free(state->tags);
}

TEMPLATE_FUNCTION(TFTagsHeadState, tf_tags_head,
                  tf_tags_head_prepare, NULL, tf_tags_head_call, tf_tags_head_free_state,
                  NULL);
