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


/*
  For some patterns, GPatternSpec stores the pattern as reversed
  string. In such cases, the matching must be executed against the
  reversed matchstring. If one uses g_pattern_match_string, glib will
  reverse the string internally in these cases, but it is suboptimal if
  the same matchstring must be matched against different patterns,
  because memory is allocated each time and string is copied as
  reversed, though it would be enough to execute this reverse once. For
  that reason one can use g_pattern_match(), which accepts both the
  matchstring and a reversed matchstring as parameters.

  Though there are cases when no reverse is needed at all. This is
  decided in the constructor of GPatternSpec, but at this time of
  writing this information is not exported, and cannot be extracted in
  any safe way, because GPatternSpec is forward declared.

  This function below is an oversimplified/safe version of the logic
  used glib to decide if the pattern will be reversed or not. One need
  to note the worst case scenario is if syslog-ng will not reverse the
  matchstring, though it should because in that case number of extra
  memory allocations and string reversals scale linearly with the number
  of patterns. We need to avoid not pre-reversing, when glib would.
 */
gboolean
joker_or_wildcard(GList *patterns)
{
  gboolean retval = FALSE;
  GList *pattern = patterns;
  while (pattern != NULL)
    {
      gchar *str = ((gchar *)pattern->data);
      if (strpbrk(str, "*?"))
        {
          retval = TRUE;
          break;
        }
      pattern = g_list_next(pattern);
    }

  return retval;
}

static void
_compile_and_add(gpointer tag_glob, gpointer exclude_patterns)
{
  g_ptr_array_add(exclude_patterns, g_pattern_spec_new(tag_glob));
}

static void
xml_scanner_options_compile_exclude_tags_to_patterns(XMLScannerOptions *self)
{
  g_ptr_array_free(self->exclude_patterns, TRUE);
  self->exclude_patterns = g_ptr_array_new_with_free_func((GDestroyNotify)g_pattern_spec_free);
  g_list_foreach(self->exclude_tags, _compile_and_add, self->exclude_patterns);
  self->matchstring_shouldreverse = joker_or_wildcard(self->exclude_tags);
}

void
xml_scanner_options_set_and_compile_exclude_tags(XMLScannerOptions *self, GList *exclude_tags)
{
  g_list_free_full(self->exclude_tags, g_free);
  self->exclude_tags = g_list_copy_deep(exclude_tags, ((GCopyFunc)g_strdup), NULL);
  xml_scanner_options_compile_exclude_tags_to_patterns(self);
}

void
xml_scanner_options_set_strip_whitespaces(XMLScannerOptions *self, gboolean setting)
{
  self->strip_whitespaces = setting;
}

void
xml_scanner_options_destroy(XMLScannerOptions *self)
{
  g_list_free_full(self->exclude_tags, g_free);
  self->exclude_tags = NULL;
  g_ptr_array_free(self->exclude_patterns, TRUE);
  self->exclude_patterns = NULL;
}

void
xml_scanner_options_copy(XMLScannerOptions *dest, XMLScannerOptions *source)
{
  xml_scanner_options_set_strip_whitespaces(dest, source->strip_whitespaces);
  xml_scanner_options_set_and_compile_exclude_tags(dest, source->exclude_tags);
}

void
xml_scanner_options_defaults(XMLScannerOptions *self)
{
  self->exclude_patterns = g_ptr_array_new_with_free_func((GDestroyNotify)g_pattern_spec_free);
  self->strip_whitespaces = FALSE;
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

  if (scanner->options->matchstring_shouldreverse)
    {
      reversed = g_utf8_strreverse(element_name, tag_length);
    }

  if (tag_matches_patterns(scanner->options->exclude_patterns, tag_length, element_name, reversed))
    {
      msg_debug("xml: subtree skipped",
                evt_tag_str("tag", element_name));
      scanner->pop_next_time = 1;
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

  if (scanner->pop_next_time)
    {
      g_markup_parse_context_pop(context);
      scanner->pop_next_time = 0;
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
_append_text(const gchar *current, const gchar *text, gsize text_len, gboolean strip_whitespaces)
{
  if (strip_whitespaces)
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

  GString *value = _append_text(current_value, text, text_len, scanner->options->strip_whitespaces);
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
xml_scanner_init(XMLScanner *self, InserterState *state, XMLScannerOptions *options)
{
  memset(self, 0, sizeof(*self));
  self->state = state;
  self->options = options;
  self->xml_ctx = g_markup_parse_context_new(&xml_scanner, 0, self, NULL);
}

void
xml_scanner_deinit(XMLScanner *self)
{
  g_markup_parse_context_free(self->xml_ctx);
  self->state = NULL; // ownership
  self->options = NULL;
}
