/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx-parse-xml.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-primitive.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"
#include "scratch-buffers.h"

static GQuark
_error_quark(void)
{
  return g_quark_from_static_string("filterx-parse-xml");
}

enum FilterXParseXmlErrorCode
{
  FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
};

static FilterXObject *
_create_list_and_move(FilterXObject *parent_obj, FilterXObject *obj)
{
  FilterXObject *list = filterx_object_create_list(parent_obj);
  g_assert(list);
  g_assert(filterx_list_append(list, obj));
  return list;
}

static FilterXObject *
_prepare_inner_object_for_start_element(const gchar *element_name, FilterXObject *current_obj,
                                        guint64 *num_of_elems, GError **error)
{
  FilterXObject *inner_obj_key = filterx_string_new(element_name, -1);
  FilterXObject *inner_obj = NULL;

  if (!filterx_object_has_subscript(current_obj, inner_obj_key))
    {
      /*
       * This is the first element, we store it as a string, and convert to a list if more elements come,
       * or to a dict if it is not a leaf, but a node.
       *
       * _text() does not get called if there is no text value, and we want to see "" if there is no text.
       * Let's set the inner object to an empty string.
       */
      *num_of_elems = 1;
      inner_obj = filterx_string_new("", 0);
      if (!filterx_object_set_subscript(current_obj, inner_obj_key, inner_obj))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert empty string: %s=\"\"", element_name);
          filterx_object_unref(inner_obj);
          inner_obj = NULL;
        }
      goto exit;
    }

  FilterXObject *existing_inner_obj = filterx_object_get_subscript(current_obj, inner_obj_key);
  if (filterx_object_is_type(existing_inner_obj, &FILTERX_TYPE_NAME(string)))
    {
      /*
       * There is already a string here.
       * Let's transform the inner object to a list and add the existing value and a new empty string.
       */
      inner_obj = _create_list_and_move(current_obj, existing_inner_obj);
      g_assert(filterx_list_append(inner_obj, filterx_string_new("", 0)));
      *num_of_elems = filterx_list_len(inner_obj);

      if (!filterx_object_set_subscript(current_obj, inner_obj_key, inner_obj))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert list: %s=[..., \"%s\"]",
                      element_name, filterx_string_get_value(existing_inner_obj, NULL));
          filterx_object_unref(inner_obj);
          inner_obj = NULL;
        }

      goto exit;
    }

  if (filterx_object_is_type(existing_inner_obj, &FILTERX_TYPE_NAME(list)))
    {
      /*
       * The inner object is already a list with values.
       * Let's add a new empty string.
       */
      inner_obj = existing_inner_obj;
      *num_of_elems = filterx_list_len(inner_obj) + 1;

      if (!filterx_list_append(inner_obj, filterx_string_new("", 0)))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to append to list: \"\"");
          filterx_object_unref(inner_obj);
          inner_obj = NULL;
        }

      goto exit;
    }

  msg_debug("FilterX: parse_xml(): Unexpected node type, removing", evt_tag_str("type", existing_inner_obj->type->name));
  filterx_object_unref(existing_inner_obj);
  g_assert(filterx_object_del_subscript(current_obj, inner_obj_key));
  inner_obj = _prepare_inner_object_for_start_element(element_name, current_obj, num_of_elems, error);

exit:
  filterx_object_unref(inner_obj_key);
  return inner_obj;
}

static FilterXObject *
_create_attrs_obj_key(const gchar *element_name)
{
  GString *attrs_obj_key_str = scratch_buffers_alloc();
  g_string_append_printf(attrs_obj_key_str, "%s.attrs", element_name);
  return filterx_string_new(attrs_obj_key_str->str, attrs_obj_key_str->len);
}

static void
_prepare_dict_for_attribute_collection_no_attrs(const gchar *element_name, FilterXObject *current_obj,
                                                guint64 num_of_elems, GError **error)
{
  g_assert(num_of_elems > 0);

  if (num_of_elems == 1)
    {
      /*
       * As this is the first element, we do nothing, maybe we won't have more elements, and we don't want to create
       * empty lists and dicts if it is not necessary.
       */
      return;
    }

  FilterXObject *attrs_obj_key = _create_attrs_obj_key(element_name);

  /*
   * This is not the first element.
   * We might have a dict, a list of dicts or nothing.
   */

  if (!filterx_object_has_subscript(current_obj, attrs_obj_key))
    {
      /*
       * We have nothing, so the previous elements didn't have any attrs either.
       * Don't do anything.
       */
      return;
    }

  FilterXObject *existing_obj = filterx_object_get_subscript(current_obj, attrs_obj_key);
  if (filterx_object_is_type(existing_obj, &FILTERX_TYPE_NAME(dict)))
    {
      /*
       * We have a dict.
       * Let's replace it with a list, add the existing dict to it, and add a null to show that this element
       * does not have attributes.
       */
      FilterXObject *new_list = _create_list_and_move(current_obj, existing_obj);
      FilterXObject *null_object = filterx_null_new();

      g_assert(filterx_list_append(new_list, null_object));
      filterx_object_unref(null_object);

      if (!filterx_object_set_subscript(current_obj, attrs_obj_key, new_list))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert list: %s=[{...}, null]", filterx_string_get_value(attrs_obj_key, NULL));
        }
      filterx_object_unref(new_list);
      goto exit;
    }

  if (filterx_object_is_type(existing_obj, &FILTERX_TYPE_NAME(list)))
    {
      /*
       * We have a list already.
       * Let's add a null to show that this element does not have attributes.
       */
      FilterXObject *null_object = filterx_null_new();
      if (!filterx_list_append(existing_obj, null_object))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to append to list: null");
        }

      filterx_object_unref(null_object);
      filterx_object_unref(existing_obj);
      goto exit;
    }

  msg_debug("FilterX: parse_xml(): Unexpected node type, removing", evt_tag_str("type", existing_obj->type->name));
  filterx_object_unref(existing_obj);
  g_assert(filterx_object_del_subscript(current_obj, attrs_obj_key));

exit:
  filterx_object_unref(attrs_obj_key);
}

static FilterXObject *
_prepare_dict_for_attribute_collection_has_attrs(const gchar *element_name, FilterXObject *current_obj,
                                                 guint64 num_of_elems, GError **error)
{
  /*
   * The element has some attributes.
   * We might have a dict, a list of dicts or nothing.
   */

  FilterXObject *dict = NULL;
  FilterXObject *attrs_obj_key = _create_attrs_obj_key(element_name);

  if (num_of_elems == 1)
    {
      /*
       * First element, create a dict for the attrs.
       */
      dict = filterx_object_create_dict(current_obj);
      g_assert(dict);

      if (!filterx_object_set_subscript(current_obj, attrs_obj_key, dict))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert empty dict: %s={}", filterx_string_get_value(attrs_obj_key, NULL));
          filterx_object_unref(dict);
          dict = NULL;
        }

      goto exit;
    }

  if (!filterx_object_has_subscript(current_obj, attrs_obj_key))
    {
      /*
       * We have nothing, so the previous elements didn't have any attrs either.
       * Don't do anything.
       */
      goto exit;
    }

  FilterXObject *existing_obj = filterx_object_get_subscript(current_obj, attrs_obj_key);
  if (filterx_object_is_type(existing_obj, &FILTERX_TYPE_NAME(dict)))
    {
      /*
       * We have a dict.
       * Let's replace it with a list, add the existing dict to it, and add a new dict where we will collect the attrs.
       */
      FilterXObject *new_list = _create_list_and_move(current_obj, existing_obj);
      if (!filterx_object_set_subscript(current_obj, attrs_obj_key, new_list))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert empty list: %s=[]", filterx_string_get_value(attrs_obj_key, NULL));
          filterx_object_unref(new_list);
          goto exit;
        }

      dict = filterx_object_create_dict(new_list);
      g_assert(dict);
      g_assert(filterx_list_append(new_list, dict));

      filterx_object_unref(new_list);
      goto exit;
    }

  if (filterx_object_is_type(existing_obj, &FILTERX_TYPE_NAME(list)))
    {
      /*
       * We have a list already.
       * Let's add a new dict where we will collect the attrs.
       */
      dict = filterx_object_create_dict(existing_obj);
      g_assert(dict);

      if (!filterx_list_append(existing_obj, dict))
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to append empty dict to list: {}");
          filterx_object_unref(dict);
          dict = NULL;
        }

      filterx_object_unref(existing_obj);
      goto exit;
    }

  msg_debug("FilterX: parse_xml(): Unexpected node type, removing", evt_tag_str("type", existing_obj->type->name));
  filterx_object_unref(existing_obj);
  g_assert(filterx_object_del_subscript(current_obj, attrs_obj_key));

exit:
  filterx_object_unref(attrs_obj_key);
  return dict;
}

static FilterXObject *
_prepare_dict_for_attribute_collection(const gchar *element_name, FilterXObject *current_obj,
                                       const gchar **attribute_names, const gchar **attribute_values,
                                       guint64 num_of_elems, GError **error)
{
  gboolean has_attrs = !!attribute_names[0];
  if (!has_attrs)
    {
      _prepare_dict_for_attribute_collection_no_attrs(element_name, current_obj, num_of_elems, error);
      return NULL;
    }

  return _prepare_dict_for_attribute_collection_has_attrs(element_name, current_obj, num_of_elems, error);
}

static void
_collect_element_attributes(const gchar *element_name, FilterXObject *current_obj,
                            const gchar **attribute_names, const gchar **attribute_values,
                            guint64 num_of_elems, GError **error)
{
  FilterXObject *dict = _prepare_dict_for_attribute_collection(element_name, current_obj,
                        attribute_names, attribute_values, num_of_elems, error);
  if (!dict)
    return;

  gint i = 0;
  while (TRUE)
    {
      const gchar *attr_key = attribute_names[i];
      if (!attr_key)
        break;
      const gchar *attr_value = attribute_values[i];

      FilterXObject *key = filterx_string_new(attr_key, -1);
      FilterXObject *value = filterx_string_new(attr_value, -1);

      gboolean success = filterx_object_set_subscript(dict, key, value);

      filterx_object_unref(key);
      filterx_object_unref(value);

      if (!success)
        {
          g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                      "Failed to insert element attribute to dict: %s=%s", attr_key, attr_value);
          break;
        }
      i++;
    }

  filterx_object_unref(dict);
}

static FilterXObject *
_ensure_dict(GMarkupParseContext *context, GQueue *obj_stack, GError **error)
{
  FilterXObject *dict = NULL;
  FilterXObject *current_obj = g_queue_peek_head(obj_stack);

  if (filterx_object_is_type(current_obj, &FILTERX_TYPE_NAME(dict)))
    return current_obj;

  FilterXObject *parent_obj = g_queue_peek_nth(obj_stack, 1);
  g_assert(filterx_object_is_type(parent_obj, &FILTERX_TYPE_NAME(dict)));
  const gchar *parent_element_name = (const gchar *) g_markup_parse_context_get_element_stack(context)->next->data;
  FilterXObject *key = filterx_string_new(parent_element_name, -1);

  if (!filterx_object_is_type(current_obj, &FILTERX_TYPE_NAME(string)))
    {
      msg_debug("FilterX: parse_xml(): unexpected node type, removing", evt_tag_str("type", current_obj->type->name));
      g_assert(filterx_object_del_subscript(parent_obj, key));
    }
  else
    {
      gsize len = 0;
      const gchar *existing_text = filterx_string_get_value(current_obj, &len);
      if (len != 0)
        {
          /*
          * There is a non-empty string here already.
          * This must have been here before starting to parse the XML.
          * We cannot do better than to just overwrite it.
          */
          msg_debug("FilterX: parse_xml(): unexpected text, overwriting", evt_tag_str("text", existing_text));
        }
    }

  /*
   * We turned out to be a non-leaf node instead of a leaf node.
   * Let's transform the empty string to an empty dict.
   */

  dict = filterx_object_create_dict(parent_obj);
  g_assert(dict);

  if (!filterx_object_set_subscript(parent_obj, key, dict))
    {
      g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                  "Failed to insert empty dict: %s={}", parent_element_name);
      filterx_object_unref(dict);
      dict = NULL;
      goto exit;
    }

  g_queue_pop_head(obj_stack);
  g_queue_push_head(obj_stack, dict);

exit:
  filterx_object_unref(key);
  return dict;
}

static void
_start_element(GMarkupParseContext *context, const gchar *element_name,
               const gchar **attribute_names, const gchar **attribute_values,
               gpointer user_data, GError **error)
{
  GQueue *obj_stack = (GQueue *) user_data;
  FilterXObject *current_obj = _ensure_dict(context, obj_stack, error);
  if (!current_obj)
    return;

  guint64 num_of_elems;
  FilterXObject *inner_obj = _prepare_inner_object_for_start_element(element_name, current_obj, &num_of_elems, error);
  g_queue_push_head(obj_stack, inner_obj);

  _collect_element_attributes(element_name, current_obj, attribute_names, attribute_values, num_of_elems, error);
}

void
_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  GQueue *obj_stack = (GQueue *) user_data;

  FilterXObject *current_obj = g_queue_pop_head(obj_stack);
  filterx_object_unref(current_obj);
}

static FilterXObject *
_create_text_object(const gchar *text, gsize text_len)
{
  gchar *stripped_text = g_strndup(text, text_len);
  g_strstrip(stripped_text);

  gsize stripped_text_len = strlen(stripped_text);
  if (!stripped_text_len)
    {
      g_free(stripped_text);
      return NULL;
    }

  FilterXObject *result = filterx_string_new(stripped_text, stripped_text_len);
  g_free(stripped_text);
  return result;
}

static void
_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  GQueue *obj_stack = (GQueue *) user_data;
  FilterXObject *parent_obj = g_queue_peek_nth(obj_stack, 1);
  FilterXObject *current_obj = g_queue_peek_head(obj_stack);
  const gchar *element_name = g_markup_parse_context_get_element(context);

  FilterXObject *text_obj = _create_text_object(text, text_len);
  if (!text_obj)
    return; // Nothing to do

  if (filterx_object_is_type(current_obj, &FILTERX_TYPE_NAME(string)))
    {
      g_assert(filterx_object_is_type(parent_obj, &FILTERX_TYPE_NAME(dict)));

      FilterXObject *key = filterx_string_new(element_name, -1);
      gboolean result = filterx_object_set_subscript(parent_obj, key, text_obj);

      filterx_object_unref(key);
      filterx_object_unref(text_obj);

      if (!result)
        g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                    "Failed to add text to dict: %s=%s", element_name, text);
      return;
    }

  g_assert(filterx_object_is_type(current_obj, &FILTERX_TYPE_NAME(list)));

  gboolean result = filterx_list_set_subscript(current_obj, -1, text_obj);
  filterx_object_unref(text_obj);

  if (!result)
    g_set_error(error, _error_quark(), FILTERX_PARSE_XML_ERROR_CODE_PARSE_ERROR,
                "Failed to add text to list: %s", text);
}

static gboolean
_parse_xml(const gchar *input, gssize input_len, FilterXObject *output_dict)
{
  GMarkupParser scanner_callbacks =
  {
    .start_element = _start_element,
    .end_element = _end_element,
    .text = _text
  };

  GQueue *obj_stack = g_queue_new();
  g_queue_push_head(obj_stack, output_dict);

  GMarkupParseContext *context = g_markup_parse_context_new(&scanner_callbacks, 0, obj_stack, NULL);

  GError *error = NULL;
  if (!g_markup_parse_context_parse(context, input, input_len, &error) ||
      !g_markup_parse_context_end_parse(context, &error))
    {
      msg_error("FilterX: parse_xml(): Failed to parse XML",
                evt_tag_str("error", error->message));
      g_error_free(error);
      g_markup_parse_context_free(context);
      g_queue_free(obj_stack);
      return FALSE;
    }

  g_markup_parse_context_free(context);
  g_queue_free(obj_stack);
  return TRUE;
}

FilterXObject *
filterx_parse_xml_new(FilterXObject *fillable, GPtrArray *args)
{
  if (!args || args->len != 1)
    {
      msg_error("FilterX: parse_xml(): Invalid number of arguments. "
                "Usage: parse_xml($raw_xml)");
      return NULL;
    }

  FilterXObject *raw_xml_obj = (FilterXObject *) g_ptr_array_index(args, 0);
  gsize raw_xml_len;
  const gchar *raw_xml = filterx_string_get_value(raw_xml_obj, &raw_xml_len);
  if (!raw_xml)
    {
      msg_error("FilterX: parse_xml(): Invalid arguments. First argument must be string type. "
                "Usage: parse_xml($raw_xml)",
                evt_tag_str("type", raw_xml_obj->type->name));
      return NULL;
    }

  return filterx_boolean_new(_parse_xml(raw_xml, raw_xml_len, fillable));
}

gpointer
filterx_parse_xml_new_construct(Plugin *self)
{
  return (gpointer) &filterx_parse_xml_new;
}
