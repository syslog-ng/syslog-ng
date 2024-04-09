/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx.h"
#include "filterx/object-json.h"

FilterXObject *
filterx_test_dict_new(void)
{
  FilterXObject *result = filterx_json_object_new_empty();
  result->type = &FILTERX_TYPE_NAME(test_dict);
  return result;
}

FilterXObject *
filterx_test_list_new(void)
{
  FilterXObject *result = filterx_json_array_new_empty();
  result->type = &FILTERX_TYPE_NAME(test_list);
  return result;
}

const gchar *
filterx_test_unknown_object_marshaled_repr(gssize *len)
{
  static const gchar *marshaled = "test_unknown_object_marshaled";
  if (len)
    *len = strlen(marshaled);
  return marshaled;
}

const gchar *
filterx_test_unknown_object_repr(gssize *len)
{
  static const gchar *repr = "test_unknown_object_repr";
  if (len)
    *len = strlen(repr);
  return repr;
}

static gboolean
_unknown_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  *t = LM_VT_STRING;
  g_string_append(repr, filterx_test_unknown_object_marshaled_repr(NULL));
  return TRUE;
}

static gboolean
_unknown_repr(FilterXObject *s, GString *repr)
{
  g_string_append(repr, filterx_test_unknown_object_repr(NULL));
  return TRUE;
}

static gboolean
_unknown_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_unknown_map_to_json(FilterXObject *s, struct json_object **object)
{
  gssize len;
  const gchar *repr = filterx_test_unknown_object_marshaled_repr(&len);
  *object = json_object_new_string_len(repr, len);
  return TRUE;
}

FilterXObject *
filterx_test_unknown_object_new(void)
{
  return filterx_object_new(&FILTERX_TYPE_NAME(test_unknown_object));
}

void
init_libtest_filterx(void)
{
  filterx_type_init(&FILTERX_TYPE_NAME(test_unknown_object));

  /* These will use the json implementations, but won't be json typed. */
  filterx_type_init(&FILTERX_TYPE_NAME(test_dict));
  filterx_type_init(&FILTERX_TYPE_NAME(test_list));
  FILTERX_TYPE_NAME(test_dict) = FILTERX_TYPE_NAME(json_object);
  FILTERX_TYPE_NAME(test_list) = FILTERX_TYPE_NAME(json_array);
}

void
deinit_libtest_filterx(void)
{
}

FILTERX_DEFINE_TYPE(test_dict, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(test_list, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(test_unknown_object, FILTERX_TYPE_NAME(object),
                    .is_mutable = FALSE,
                    .truthy = _unknown_truthy,
                    .marshal = _unknown_marshal,
                    .repr = _unknown_repr,
                    .map_to_json = _unknown_map_to_json,
                    .list_factory = filterx_test_list_new,
                    .dict_factory = filterx_test_dict_new);
