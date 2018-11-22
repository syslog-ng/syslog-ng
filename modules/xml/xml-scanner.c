/*
 * Copyright (c) 2018 Balabit
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

#include "xml-scanner.h"
#include "scratch-buffers.h"

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
tag_matches_patterns(const GPtrArray *patterns, const gint tag_length,
                     const gchar *element_name, const gchar *reversed_name)
{
  for (int i = 0; i < patterns->len; i++)
    if (g_pattern_match((GPatternSpec *)g_ptr_array_index(patterns, i),
                        tag_length, element_name, reversed_name))
      {
        return TRUE;
      }

  return FALSE;
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
  XMLScanner *scanner = (XMLScanner *)user_data;
  InserterState *state = scanner->state;

  gchar *reversed = NULL;
  guint tag_length = strlen(element_name);

  if (state->parser->matchstring_shouldreverse)
    {
      reversed = g_utf8_strreverse(element_name, tag_length);
    }

  if (tag_matches_patterns(state->parser->exclude_patterns, tag_length, element_name, reversed))
    {
      msg_debug("xml: subtree skipped",
                evt_tag_str("tag", element_name));
      state->pop_next_time = 1;
      g_markup_parse_context_push(context, &skip, NULL);
      g_free(reversed);
      return;
    }

  g_string_append_c(state->key, '.');
  g_string_append(state->key, element_name);
  msg_add_attributes(state->msg, state->key, attribute_names, attribute_values);

  g_free(reversed);
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
  XMLScanner *scanner = (XMLScanner *)user_data;
  InserterState *state = scanner->state;

  if (state->pop_next_time)
    {
      g_markup_parse_context_pop(context);
      state->pop_next_time = 0;
      return;
    }
  g_string_truncate(state->key, before_last_dot(state->key));
}

static GString *
_append_strip(const gchar *current, const gchar *text, gsize text_len)
{
  gchar *text_to_append = NULL;
  text_to_append = g_strndup(text, text_len);
  g_strstrip(text_to_append);

  if (!text_to_append[0])
    {
      g_free(text_to_append);
      return NULL;
    }

  GString *value = scratch_buffers_alloc();
  g_string_assign(value, current);
  g_string_append(value, text_to_append);
  g_free(text_to_append);

  return value;
}

static GString *
_append_text(const gchar *current, const gchar *text, gsize text_len, XMLParser *parser)
{
  if (parser->strip_whitespaces)
    {
      return _append_strip(current, text, text_len);
    }
  else
    {
      GString *value = scratch_buffers_alloc();
      g_string_assign(value, current);
      g_string_append_len(value, text, text_len);
      return value;
    }
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

  XMLScanner *scanner = (XMLScanner *)user_data;
  InserterState *state = scanner->state;
  const gchar *current_value = log_msg_get_value_by_name(state->msg, state->key->str, NULL);

  GString *value = _append_text(current_value, text, text_len, state->parser);
  if (value)
    log_msg_set_value_by_name(state->msg, state->key->str, value->str, value->len);
}

void
xml_scanner_parse(XMLScanner *self, const gchar *input, gsize input_len, GError **error)
{
  g_markup_parse_context_parse(self->xml_ctx, input, input_len, error);
}

void
xml_scanner_end_parse(XMLScanner *self, GError **error)
{
  g_markup_parse_context_end_parse(self->xml_ctx, error);
}

static GMarkupParser xml_scanner =
{
  .start_element = start_element_cb,
  .end_element = end_element_cb,
  .text = text_cb
};

void
xml_scanner_init(XMLScanner *self, InserterState *state)
{
  memset(self, 0, sizeof(*self));
  self->state = state;
  self->xml_ctx = g_markup_parse_context_new(&xml_scanner, 0, self, NULL);
}

void
xml_scanner_deinit(XMLScanner *self)
{
  g_markup_parse_context_free(self->xml_ctx);
  self->state = NULL; // ownership
}
