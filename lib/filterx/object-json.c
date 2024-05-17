/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "filterx/object-json-internal.h"
#include "filterx/object-null.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-message-value.h"
#include "filterx/filterx-weakrefs.h"
#include "filterx/filterx-eval.h"

#include "scanner/list-scanner/list-scanner.h"
#include "str-repr/encode.h"

static int
_deep_copy_filterx_object_ref(json_object *src, json_object *parent, const char *key, size_t index, json_object **dst)
{
  int result = json_c_shallow_copy_default(src, parent, key, index, dst);

  if (*dst != NULL)
    {
      /* we need to copy the userdata for primitive types */

      switch (json_object_get_type(src))
        {
        case json_type_null:
        case json_type_boolean:
        case json_type_double:
        case json_type_int:
        case json_type_string:
        {
          FilterXObject *fobj = json_object_get_userdata(src);
          if (fobj)
            filterx_json_associate_cached_object(*dst, fobj);
          break;
        }
        default:
          break;
        }
      return 2;
    }
  return result;
}

struct json_object *
filterx_json_deep_copy(struct json_object *jso)
{
  struct json_object *clone = NULL;
  if (json_object_deep_copy(jso, &clone, _deep_copy_filterx_object_ref) != 0)
    return NULL;

  return clone;
}

static FilterXObject *
_convert_json_to_object(FilterXObject *self, FilterXWeakRef *root_container, struct json_object *jso)
{
  switch (json_object_get_type(jso))
    {
    case json_type_null:
      return filterx_null_new();
    case json_type_double:
      return filterx_double_new(json_object_get_double(jso));
    case json_type_boolean:
      return filterx_boolean_new(json_object_get_boolean(jso));
    case json_type_int:
      return filterx_integer_new(json_object_get_int64(jso));
    case json_type_string:
      return filterx_string_new(json_object_get_string(jso), -1);
    case json_type_array:
      return filterx_json_array_new_sub(json_object_get(jso),
                                        filterx_weakref_get(root_container) ? : filterx_object_ref(self));
    case json_type_object:
      return filterx_json_object_new_sub(json_object_get(jso),
                                         filterx_weakref_get(root_container) ? : filterx_object_ref(self));
    default:
      g_assert_not_reached();
    }
}

FilterXObject *
filterx_json_convert_json_to_object_cached(FilterXObject *self, FilterXWeakRef *root_container,
                                           struct json_object *jso)
{
  FilterXObject *filterx_obj;

  if (!jso || json_object_get_type(jso) == json_type_null)
    return filterx_null_new();

  if (json_object_get_type(jso) == json_type_double)
    {
      /* this is a workaround to ditch builtin serializer for double objects
       * that are set when parsing from a string representation.
       * json_object_double_new_ds() will set userdata to the string
       * representation of the number, as extracted from the JSON source.
       *
       * Changing the value of the double to the same value, ditches this,
       * but only if necessary.
       *
       * This only works starting with 0.14, before that json_object_set_double()
       * does not drop the string user data, so we cannot check if it is our
       * filterx object or the string, which means we cannot cache doubles.
       */

      if (JSON_C_MAJOR_VERSION == 0 && JSON_C_MINOR_VERSION < 14)
        return _convert_json_to_object(self, root_container, jso);

      json_object_set_double(jso, json_object_get_double(jso));
    }

  /* NOTE: userdata is a weak reference */
  filterx_obj = json_object_get_userdata(jso);
  if (filterx_obj)
    return filterx_object_ref(filterx_obj);

  filterx_obj = _convert_json_to_object(self, root_container, jso);
  if (!filterx_object_is_frozen(self))
    filterx_json_associate_cached_object(jso, filterx_obj);
  return filterx_obj;
}

void
filterx_json_associate_cached_object(struct json_object *jso, FilterXObject *filterx_obj)
{
  /* null JSON value turns into NULL pointer. */
  if (!jso)
    return;

  filterx_eval_store_weak_ref(filterx_obj);

  /* we are not storing a reference in userdata to avoid circular
   * references.  That ref is retained by the filterx_scope_store_weak_ref()
   * call above, which is automatically terminated when the scope ends */
  json_object_set_userdata(jso, filterx_obj, NULL);
}

FilterXObject *
filterx_json_new_from_repr(const gchar *repr, gssize repr_len)
{
  if (repr_len == 0 || repr[0] == '{')
    return filterx_json_object_new_from_repr(repr, repr_len);
  return filterx_json_array_new_from_repr(repr, repr_len);
}

FilterXObject *
filterx_json_new_from_args(GPtrArray *args)
{
  if (!args || args->len == 0)
    return filterx_json_object_new_empty();

  if (args->len != 1)
    {
      msg_error("FilterX: Failed to create JSON object: invalid number of arguments. "
                "Usage: json() or json($raw_json_string) or json($existing_json)");
      return NULL;
    }

  FilterXObject *arg = (FilterXObject *) g_ptr_array_index(args, 0);

  if (filterx_object_is_type(arg, &FILTERX_TYPE_NAME(json_array)) ||
      filterx_object_is_type(arg, &FILTERX_TYPE_NAME(json_object)))
    return filterx_object_ref(arg);

  if (filterx_object_is_type(arg, &FILTERX_TYPE_NAME(message_value)))
    {
      FilterXObject *unmarshalled = filterx_object_unmarshal(arg);
      if (!filterx_object_is_type(unmarshalled, &FILTERX_TYPE_NAME(json_array)) &&
          !filterx_object_is_type(unmarshalled, &FILTERX_TYPE_NAME(json_object)))
        {
          filterx_object_unref(unmarshalled);
          goto error;
        }
      return unmarshalled;
    }

  gsize repr_len;
  const gchar *repr = filterx_string_get_value(arg, &repr_len);
  if (repr)
    return filterx_json_new_from_repr(repr, repr_len);

error:
  msg_error("FilterX: Failed to create JSON object: invalid argument type. "
            "Usage: json() or json($raw_json_string) or json($syslog_ng_list) or json($existing_json)",
            evt_tag_str("type", arg->type->name));
  return NULL;
}

FilterXObject *
filterx_json_new_from_object(struct json_object *jso)
{
  if (json_object_get_type(jso) == json_type_object)
    return filterx_json_object_new_sub(jso, NULL);

  if (json_object_get_type(jso) == json_type_array)
    return filterx_json_array_new_sub(jso, NULL);

  return NULL;
}

const gchar *
filterx_json_to_json_literal(FilterXObject *s)
{
  if (filterx_object_is_type(s, &FILTERX_TYPE_NAME(json_object)))
    return filterx_json_object_to_json_literal(s);
  if (filterx_object_is_type(s, &FILTERX_TYPE_NAME(json_array)))
    return filterx_json_array_to_json_literal(s);
  return NULL;
}
