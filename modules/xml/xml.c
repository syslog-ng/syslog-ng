/*
 * Copyright (c) 2017 Balabit
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

#include "xml.h"
#include "scratch-buffers.h"

typedef struct
{
  LogMessage *msg;
  GString *key;
  gboolean pop_next_time;
  XMLParser *parser;
} InserterState;

static void
remove_trailing_dot(gchar *str)
{
  g_assert(strlen(str));
  if (str[strlen(str)-1] == '.')
    str[strlen(str)-1] = 0;
}

static void
msg_add_attributes(LogMessage *msg, GString *key, const gchar **names, const gchar **values)
{
  GString *attr_key;

  if (names[0])
    {
      attr_key = scratch_buffers_alloc();
      g_string_assign(attr_key, key->str);
      g_string_append(attr_key, "._");
    }
  else
    return;

  gint base_index = attr_key->len;
  gint attrs = 0;

  while (names[attrs])
    {
      attr_key = g_string_overwrite(attr_key, base_index, names[attrs]);
      log_msg_set_value_by_name(msg, attr_key->str, values[attrs], -1);
      attrs++;
    }
}

static gboolean
tag_matches_patterns(const gchar *element_name, const GPtrArray *patterns)
{
  gboolean retval = FALSE;
  guint tag_length = strlen(element_name);
  gchar *reversed = g_utf8_strreverse (element_name, tag_length);

  for (int i = 0; i < patterns->len; i++)
    if (g_pattern_match((GPatternSpec *)g_ptr_array_index(patterns, i),
                        tag_length, element_name, reversed))
      {
        retval = TRUE;
        break;
      }

  g_free(reversed);
  return retval;
}

static GMarkupParser skip = {};

static void
start_element_cb(GMarkupParseContext  *context,
                 const gchar          *element_name,
                 const gchar         **attribute_names,
                 const gchar         **attribute_values,
                 gpointer              user_data,
                 GError              **error)
{
  InserterState *state = (InserterState *)user_data;

  if (tag_matches_patterns(element_name, state->parser->exclude_patterns))
    {
      msg_debug("xml: subtree skipped", evt_tag_str("tag", element_name));
      state->pop_next_time = 1;
      g_markup_parse_context_push(context, &skip, NULL);
      return;
    }

  g_string_append_c(state->key, '.');
  g_string_append(state->key, element_name);
  msg_add_attributes(state->msg, state->key, attribute_names, attribute_values);
}

static gint
before_last_dot(GString *str)
{
  const gchar *s = str->str;
  gchar *pos = strrchr(s, '.');
  return (pos-s);
}

static void
end_element_cb(GMarkupParseContext *context,
               const gchar         *element_name,
               gpointer             user_data,
               GError              **error)
{
  InserterState *state = (InserterState *)user_data;

  if (state->pop_next_time)
    {
      g_markup_parse_context_pop(context);
      state->pop_next_time = 0;
      return;
    }
  g_string_truncate(state->key, before_last_dot(state->key));
}

static void
text_cb(GMarkupParseContext *context,
        const gchar         *text,
        gsize                text_len,
        gpointer             user_data,
        GError             **error)
{
  if (text_len == 0)
    return;

  InserterState *state = (InserterState *)user_data;
  const gchar *current_value = log_msg_get_value_by_name(state->msg, state->key->str, NULL);
  GString *value = scratch_buffers_alloc();
  g_string_assign(value, current_value);
  g_string_append(value, text);
  log_msg_set_value_by_name(state->msg, state->key->str, value->str, value->len);
}


static GMarkupParser xml_scanner =
{
  .start_element = start_element_cb,
  .end_element = end_element_cb,
  .text = text_cb
};

static gboolean
xml_parser_process(LogParser *s, LogMessage **pmsg,
                   const LogPathOptions *path_options,
                   const gchar *input, gsize input_len)
{
  XMLParser *self = (XMLParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);

  GString *key = scratch_buffers_alloc();
  key = g_string_append(key, self->prefix);

  InserterState state = { .msg = msg, .key = key, .parser = self };
  GMarkupParseContext *xml_ctx = g_markup_parse_context_new(&xml_scanner, 0, &state, NULL);

  GError *error = NULL;
  g_markup_parse_context_parse(xml_ctx, input, input_len, &error);
  if (error)
    goto err;

  g_markup_parse_context_end_parse(xml_ctx, &error);
  if (error)
    goto err;

  g_markup_parse_context_free(xml_ctx);
  return TRUE;

err:
  msg_error("xml: error", evt_tag_str("str", error->message));
  g_error_free(error);
  g_markup_parse_context_free(xml_ctx);
  return self->forward_invalid;
}

static void
_compile_and_add(gpointer tag_glob, gpointer exclude_patterns)
{
  g_ptr_array_add(exclude_patterns, g_pattern_spec_new(tag_glob));
}

void
xml_parser_set_exclude_tags(LogParser *s, GList *exclude_tags)
{
  XMLParser *self = (XMLParser *) s;
  g_list_foreach(exclude_tags, _compile_and_add, self->exclude_patterns);
}

void
xml_parser_set_forward_invalid(LogParser *s, gboolean setting)
{
  XMLParser *self = (XMLParser *) s;
  self->forward_invalid = setting;
}

void
xml_parser_set_prefix(LogParser *s, const gchar *prefix)
{
  XMLParser *self = (XMLParser *) s;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

static GPtrArray *
_clone_exclude_patterns(XMLParser *self)
{
  GPtrArray *array = g_ptr_array_new_full(self->exclude_patterns->len, (GDestroyNotify)g_pattern_spec_free);
  for (int i = 0; i < self->exclude_patterns->len; i++)
    g_ptr_array_add(array, g_ptr_array_index(self->exclude_patterns, i));
  return array;
}

static LogPipe *
xml_parser_clone(LogPipe *s)
{
  XMLParser *self = (XMLParser *) s;
  XMLParser *cloned;

  cloned = (XMLParser *) xml_parser_new(s->cfg);

  xml_parser_set_prefix(&cloned->super, self->prefix);
  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));
  xml_parser_set_forward_invalid(&cloned->super, self->forward_invalid);
  cloned->exclude_patterns = _clone_exclude_patterns(self);

  return &cloned->super.super;
}

static void
xml_parser_free(LogPipe *s)
{
  XMLParser *self = (XMLParser *) s;
  g_free(self->prefix);
  self->prefix = NULL;
  g_ptr_array_free(self->exclude_patterns, TRUE);
  self->exclude_patterns = NULL;

  log_parser_free_method(s);
}

static gboolean
xml_parser_init(LogPipe *s)
{
  XMLParser *self = (XMLParser *)s;
  remove_trailing_dot(self->prefix);

  return log_parser_init_method(s);
}


LogParser *
xml_parser_new(GlobalConfig *cfg)
{
  XMLParser *self = g_new0(XMLParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = xml_parser_init;
  self->super.super.free_fn = xml_parser_free;
  self->super.super.clone = xml_parser_clone;
  self->super.process = xml_parser_process;
  self->forward_invalid = TRUE;

  xml_parser_set_prefix(&self->super, ".xml");
  self->exclude_patterns = g_ptr_array_new_with_free_func((GDestroyNotify)g_pattern_spec_free);
  return &self->super;
}
