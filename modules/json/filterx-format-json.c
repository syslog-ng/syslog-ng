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

#include "filterx-format-json.h"
#include "filterx/object-message-value.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-json.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"
#include "scratch-buffers.h"
#include "utf8utils.h"

static gboolean _format_and_append_value(FilterXObject *value, GString *result);

static void
_append_comma_if_needed(GString *result)
{
  if (result->len &&
      result->str[result->len - 1] != '[' &&
      result->str[result->len - 1] != '{' &&
      result->str[result->len - 1] != ':')
    {
      g_string_append_c(result, ',');
    }
}

static gboolean
_format_and_append_message_value(FilterXObject *value, GString *result)
{
  if (filterx_message_value_get_type(value) == LM_VT_JSON)
    {
      gsize json_value_len;
      const gchar *json_value = filterx_message_value_get_value(value, &json_value_len);
      _append_comma_if_needed(result);
      g_string_append_len(result, json_value, json_value_len);
      return TRUE;
    }

  FilterXObject *unmarshalled_value = filterx_object_unmarshal(value);
  gboolean success = _format_and_append_value(unmarshalled_value, result);
  filterx_object_unref(unmarshalled_value);
  return success;
}

static gboolean
_format_and_append_json(FilterXObject *value, GString *result)
{
  _append_comma_if_needed(result);
  g_string_append(result, filterx_json_to_json_literal(value));
  return TRUE;
}

static gboolean
_format_and_append_null(GString *result)
{
  _append_comma_if_needed(result);
  g_string_append(result, "null");
  return TRUE;
}

static gboolean
_format_and_append_boolean(FilterXObject *value, GString *result)
{
  gboolean bool_value;
  g_assert(filterx_boolean_unwrap(value, &bool_value));

  _append_comma_if_needed(result);
  g_string_append(result, bool_value ? "true" : "false");
  return TRUE;
}

static gboolean
_format_and_append_integer(FilterXObject *value, GString *result)
{
  gint64 int_value;
  g_assert(filterx_integer_unwrap(value, &int_value));

  _append_comma_if_needed(result);
  g_string_append_printf(result, "%" G_GINT64_FORMAT, int_value);
  return TRUE;
}

static gboolean
_format_and_append_double(FilterXObject *value, GString *result)
{
  gdouble double_value;
  g_assert(filterx_double_unwrap(value, &double_value));

  _append_comma_if_needed(result);

  gsize init_len = result->len;
  g_string_set_size(result, init_len + G_ASCII_DTOSTR_BUF_SIZE);
  g_ascii_dtostr(result->str + init_len, G_ASCII_DTOSTR_BUF_SIZE, double_value);
  g_string_set_size(result, strchr(result->str + init_len, '\0') - result->str);
  return TRUE;
}

static inline gsize
_get_base64_encoded_size(gsize len)
{
  return (len / 3 + 1) * 4 + 4;
}

static gboolean
_append_bytes(const gchar *value, gsize len, GString *result)
{
  _append_comma_if_needed(result);
  g_string_append_c(result, '"');

  gint encode_state = 0;
  gint encode_save = 0;
  gsize init_len = result->len;

  /* expand the buffer and add space for the base64 encoded string */
  g_string_set_size(result, init_len + _get_base64_encoded_size(len));
  gsize out_len = g_base64_encode_step((const guchar *) value, len, FALSE, result->str + init_len,
                                       &encode_state, &encode_save);
  g_string_set_size(result, init_len + out_len + _get_base64_encoded_size(0));

#if !GLIB_CHECK_VERSION(2, 54, 0)
  /* See modules/basicfuncs/str-funcs.c: tf_base64encode() */
  if (((unsigned char *) &encode_save)[0] == 1)
    ((unsigned char *) &encode_save)[2] = 0;
#endif

  out_len += g_base64_encode_close(FALSE, result->str + init_len + out_len, &encode_state, &encode_save);
  g_string_set_size(result, init_len + out_len);

  g_string_append_c(result, '"');
  return TRUE;
}

static gboolean
_format_and_append_bytes(FilterXObject *value, GString *result)
{
  gsize bytes_value_len;
  const gchar *bytes_value = filterx_bytes_get_value(value, &bytes_value_len);
  return _append_bytes(bytes_value, bytes_value_len, result);
}

static gboolean
_format_and_append_protobuf(FilterXObject *value, GString *result)
{
  gsize protobuf_value_len;
  const gchar *protobuf_value = filterx_protobuf_get_value(value, &protobuf_value_len);
  return _append_bytes(protobuf_value, protobuf_value_len, result);
}

static gboolean
_format_and_append_string(FilterXObject *value, GString *result)
{
  gsize string_value_len;
  const gchar *string_value = filterx_string_get_value(value, &string_value_len);

  _append_comma_if_needed(result);
  g_string_append_c(result, '"');
  append_unsafe_utf8_as_escaped(result, string_value, string_value_len, "\"", "\\u%04x", "\\\\x%02x");
  g_string_append_c(result, '"');
  return TRUE;
}

static gboolean
_format_and_append_dict_elem(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  GString *result = (GString *) user_data;

  gsize key_str_len;
  const gchar *key_str = filterx_string_get_value(key, &key_str_len);
  if (!key_str)
    return FALSE;

  _append_comma_if_needed(result);
  g_string_append_c(result, '"');
  append_unsafe_utf8_as_escaped(result, key_str, key_str_len, "\"", "\\u%04x", "\\\\x%02x");
  g_string_append(result, "\":");

  return _format_and_append_value(value, result);
}

static gboolean
_format_and_append_dict(FilterXObject *value, GString *result)
{
  _append_comma_if_needed(result);
  g_string_append_c(result, '{');

  if (!filterx_dict_iter(value, _format_and_append_dict_elem, (gpointer) result))
    return FALSE;

  g_string_append_c(result, '}');
  return TRUE;
}

static gboolean
_format_and_append_list(FilterXObject *value, GString *result)
{
  _append_comma_if_needed(result);
  g_string_append_c(result, '[');

  guint64 list_len;
  gboolean len_success = filterx_object_len(value, &list_len);
  if (!len_success)
    return FALSE;

  for (guint64 i = 0; i < list_len; i++)
    {
      FilterXObject *elem = filterx_list_get_subscript(value, i);
      gboolean success = _format_and_append_value(elem, result);
      filterx_object_unref(elem);

      if (!success)
        return FALSE;
    }

  g_string_append_c(result, ']');
  return TRUE;
}

static gboolean
_repr_append(FilterXObject *value, GString *result)
{
  ScratchBuffersMarker marker;
  GString *repr = scratch_buffers_alloc_and_mark(&marker);

  gboolean success = filterx_object_repr(value, repr);
  if (!success)
    goto exit;

  _append_comma_if_needed(result);
  g_string_append_c(result, '"');
  append_unsafe_utf8_as_escaped(result, repr->str, repr->len, "\"", "\\u%04x", "\\\\x%02x");
  g_string_append_c(result, '"');

exit:
  scratch_buffers_reclaim_marked(marker);
  return success;
}

static gboolean
_format_and_append_value(FilterXObject *value, GString *result)
{
  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(message_value)))
    return _format_and_append_message_value(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(json_object)) ||
      filterx_object_is_type(value, &FILTERX_TYPE_NAME(json_array)))
    return _format_and_append_json(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(null)))
    return _format_and_append_null(result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(boolean)))
    return _format_and_append_boolean(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(integer)))
    return _format_and_append_integer(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(double)))
    return _format_and_append_double(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(bytes)))
    return _format_and_append_bytes(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(protobuf)))
    return _format_and_append_protobuf(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(string)))
    return _format_and_append_string(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(dict)))
    return _format_and_append_dict(value, result);

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(list)))
    return _format_and_append_list(value, result);

  /* FIXME: handle datetime based on object-datetime.c:_convert_unix_time_to_string() */

  return _repr_append(value, result);
}

static FilterXObject *
_format_json(FilterXObject *arg)
{
  FilterXObject *result = NULL;
  ScratchBuffersMarker marker;
  GString *result_string = scratch_buffers_alloc_and_mark(&marker);

  if (!_format_and_append_value(arg, result_string))
    goto exit;

  result = filterx_string_new(result_string->str, result_string->len);

exit:
  scratch_buffers_reclaim_marked(marker);
  return result;
}

FilterXObject *
filterx_format_json_new(GPtrArray *args)
{
  if (!args || args->len != 1)
    {
      msg_error("FilterX: format_json(): Invalid number of arguments. "
                "Usage: format_json($data)");
      return NULL;
    }

  FilterXObject *arg = (FilterXObject *) g_ptr_array_index(args, 0);
  return _format_json(arg);
}

gpointer
filterx_format_json_new_construct(Plugin *self)
{
  return (gpointer) &filterx_format_json_new;
}
