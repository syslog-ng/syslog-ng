/*
 * Copyright (c) 2018 Balabit
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

#include "xml-scanner.h"
#include "scratch-buffers.h"
#include "compat/glib.h"


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
scanner_push_attributes(XMLScanner *self, const gchar **attribute_names, const gchar **attribute_values)
{
  GString *attr_key;

  if (attribute_names[0])
    {
      attr_key = scratch_buffers_alloc();
      g_string_assign(attr_key, self->key->str);
      g_string_append(attr_key, "._");
    }
  else
    return;

  gint base_index = attr_key->len;
  gint attrs = 0;

  while (attribute_names[attrs])
    {
      attr_key = g_string_overwrite(attr_key, base_index, attribute_names[attrs]);
      xml_scanner_push_current_key_value(self, attr_key->str, attribute_values[attrs], -1);
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


static GString *
_pop_text_from_stack(XMLScanner *self)
{
  return g_queue_pop_head(self->text_stack);
}

static void
_push_current_text_to_stack(XMLScanner *self)
{
  g_queue_push_tail(self->text_stack, self->text);
}

static void
_start_new_text_buffer(XMLScanner *self)
{
  if (self->text)
    _push_current_text_to_stack(self);
  self->text = scratch_buffers_alloc();
}

static gint
before_last_dot(GString *str)
{
  const gchar *s = str->str;
  gchar *pos = strrchr(s, '.');
  return (pos-s);
}

static void
_clear_current_element_from_key(XMLScanner *self)
{
  g_string_truncate(self->key, before_last_dot(self->key));
}

static void
_add_current_element_to_key(XMLScanner *self, const gchar *element_name)
{
  g_string_append_c(self->key, '.');
  g_string_append(self->key, element_name);
}

static GMarkupParser skip = {};

gboolean
xml_scanner_start_element_method(XMLScanner *self,
                                 const gchar          *element_name,
                                 const gchar         **attribute_names,
                                 const gchar         **attribute_values,
                                 GError              **error)
{
  gchar *reversed = NULL;
  guint tag_length = strlen(element_name);

  if (self->options->matchstring_shouldreverse)
    {
      reversed = g_utf8_strreverse(element_name, tag_length);
    }

  if (tag_matches_patterns(self->options->exclude_patterns, tag_length, element_name, reversed))
    {
      msg_debug("xml: subtree skipped",
                evt_tag_str("tag", element_name));
      self->pop_next_time = 1;
      g_markup_parse_context_push(self->xml_ctx, &skip, NULL);
      g_free(reversed);
      return FALSE;
    }
  g_free(reversed);

  _add_current_element_to_key(self, element_name);
  return TRUE;
}

static void
_xml_scanner_start_element(GMarkupParseContext  *context,
                           const gchar          *element_name,
                           const gchar         **attribute_names,
                           const gchar         **attribute_values,
                           gpointer              user_data,
                           GError              **error)
{
  XMLScanner *self = (XMLScanner *)user_data;

  _start_new_text_buffer(self);
  if (self->start_element_cb(self, element_name, attribute_names, attribute_values, error))
    scanner_push_attributes(self, attribute_names, attribute_values);
}

void
xml_scanner_end_element_method(XMLScanner *self,
                               const gchar         *element_name,
                               GError              **error)
{
  if (self->pop_next_time)
    {
      g_markup_parse_context_pop(self->xml_ctx);
      self->pop_next_time = 0;
      return;
    }
  _clear_current_element_from_key(self);
}

void
xml_scanner_push_text_method(XMLScanner *self)
{
  if (self->text->len)
    xml_scanner_push_current_key_value(self, self->key->str, self->text->str, self->text->len);
}

void
_xml_scanner_end_element(GMarkupParseContext *context,
                         const gchar         *element_name,
                         gpointer             user_data,
                         GError              **error)
{
  XMLScanner *self = (XMLScanner *)user_data;

  self->push_text(self);
  self->end_element_cb(self, element_name, error);
  self->text = _pop_text_from_stack(self);
}

static void
_strip_and_append_text(XMLScanner *self, const gchar *text, gsize text_len)
{
  gchar *buffer = g_strndup(text, text_len);
  g_strstrip(buffer);
  g_string_append(self->text, buffer);
  g_free(buffer);
}

static void
_append_text(XMLScanner *self, const gchar *text, gsize text_len)
{
  if (self->options->strip_whitespaces)
    {
      _strip_and_append_text(self, text, text_len);
      return;
    }
  g_string_append_len(self->text, text, text_len);
}

void
xml_scanner_text_method(XMLScanner *self,
                        const gchar *element_name,
                        const gchar         *text,
                        gsize                text_len,
                        GError             **error)
{
  if (text_len == 0)
    return;

  _append_text(self, text, text_len);
}

static void
_xml_scanner_text(GMarkupParseContext *context,
                  const gchar         *text,
                  gsize                text_len,
                  gpointer             user_data,
                  GError             **error)
{
  XMLScanner *self = (XMLScanner *)user_data;

  const gchar *element_name  = g_markup_parse_context_get_element(context);
  self->text_cb(self, element_name, text, text_len, error);
}

void
xml_scanner_parse(XMLScanner *self, const gchar *input, gsize input_len, GError **error)
{
  g_assert(self->push_key_value.push_function);

  GMarkupParser scanner_callbacks = { .start_element = _xml_scanner_start_element,
                                      .end_element = _xml_scanner_end_element,
                                      .text =  _xml_scanner_text
                                    };
  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);
  self->xml_ctx = g_markup_parse_context_new(&scanner_callbacks, 0, self, NULL);
  g_markup_parse_context_parse(self->xml_ctx, input, input_len, error);
  if (error && *error)
    goto exit;
  g_markup_parse_context_end_parse(self->xml_ctx, error);

exit:
  scratch_buffers_reclaim_marked(marker);
  g_markup_parse_context_free(self->xml_ctx);
  self->xml_ctx = NULL;
}



void
xml_scanner_init(XMLScanner *self, XMLScannerOptions *options, PushCurrentKeyValueCB push_function,
                 gpointer user_data, gchar *key_prefix)
{
  memset(self, 0, sizeof(*self));
  self->start_element_cb = xml_scanner_start_element_method;
  self->end_element_cb = xml_scanner_end_element_method;
  self->text_cb = xml_scanner_text_method;
  self->push_text = xml_scanner_push_text_method;

  self->options = options;
  self->push_key_value.push_function = push_function;
  self->push_key_value.user_data = user_data;
  self->key = scratch_buffers_alloc();
  g_string_assign(self->key, key_prefix);
  self->text = NULL;
  self->text_stack = g_queue_new();
}

void
xml_scanner_deinit(XMLScanner *self)
{
  self->options = NULL;
  g_queue_free(self->text_stack);
}
