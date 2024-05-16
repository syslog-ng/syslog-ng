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
#include "filterx/filterx-weakrefs.h"
#include "filterx/object-dict-interface.h"

struct FilterXJsonObject_
{
  FilterXDict super;
  FilterXWeakRef root_container;
  struct json_object *jso;
};

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  *t = LM_VT_JSON;

  const gchar *json_repr = json_object_to_json_string_ext(self->jso, JSON_C_TO_STRING_PLAIN);
  g_string_append(repr, json_repr);

  return TRUE;
}

static gboolean
_repr(FilterXObject *s, GString *repr)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  const gchar *json_repr = json_object_to_json_string_ext(self->jso, JSON_C_TO_STRING_PLAIN);
  g_string_append(repr, json_repr);

  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **jso, FilterXObject **assoc_object)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  *jso = json_object_get(self->jso);
  return TRUE;
}

static FilterXObject *
_clone(FilterXObject *s)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  struct json_object *jso = filterx_json_deep_copy(self->jso);
  if (!jso)
    return NULL;

  return filterx_json_object_new_sub(jso, NULL);
}

static FilterXObject *
_get_subscript(FilterXDict *s, FilterXObject *key)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  const gchar *key_str = filterx_string_get_value(key, NULL);
  if (!key_str)
    return NULL;

  struct json_object *jso = NULL;
  if (!json_object_object_get_ex(self->jso, key_str, &jso))
    return NULL;

  return filterx_json_convert_json_to_object_cached(&s->super, &self->root_container, jso);
}

static gboolean
_set_subscript(FilterXDict *s, FilterXObject *key, FilterXObject **new_value)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  const gchar *key_str = filterx_string_get_value(key, NULL);
  if (!key_str)
    return FALSE;

  struct json_object *jso = NULL;
  FilterXObject *assoc_object = NULL;
  if (!filterx_object_map_to_json(*new_value, &jso, &assoc_object))
    return FALSE;

  filterx_json_associate_cached_object(jso, assoc_object);

  if (json_object_object_add(self->jso, key_str, jso) != 0)
    {
      filterx_object_unref(assoc_object);
      json_object_put(jso);
      return FALSE;
    }

  self->super.super.modified_in_place = TRUE;
  FilterXObject *root_container = filterx_weakref_get(&self->root_container);
  if (root_container)
    {
      root_container->modified_in_place = TRUE;
      filterx_object_unref(root_container);
    }

  filterx_object_unref(*new_value);
  *new_value = assoc_object;

  return TRUE;
}

static gboolean
_unset_key(FilterXDict *s, FilterXObject *key)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  const gchar *key_str = filterx_string_get_value(key, NULL);
  if (!key_str)
    return FALSE;

  json_object_object_del(self->jso, key_str);

  self->super.super.modified_in_place = TRUE;
  FilterXObject *root_container = filterx_weakref_get(&self->root_container);
  if (root_container)
    {
      root_container->modified_in_place = TRUE;
      filterx_object_unref(root_container);
    }

  return TRUE;
}

static guint64
_len(FilterXDict *s)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  return json_object_object_length(self->jso);
}

static gboolean
_iter_inner(FilterXJsonObject *self, const gchar *obj_key, struct json_object *jso,
            FilterXDictIterFunc func, gpointer user_data)
{
  FilterXObject *key = filterx_string_new(obj_key, -1);
  FilterXObject *value = filterx_json_convert_json_to_object_cached(&self->super.super, &self->root_container,
                         jso);

  gboolean result = func(key, value, user_data);

  filterx_object_unref(key);
  filterx_object_unref(value);
  return result;
}

static gboolean
_iter(FilterXDict *s, FilterXDictIterFunc func, gpointer user_data)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  struct json_object_iter itr;
  json_object_object_foreachC(self->jso, itr)
  {
    if (!_iter_inner(self, itr.key, itr.val, func, user_data))
      return FALSE;
  }
  return TRUE;
}

/* NOTE: consumes root ref */
FilterXObject *
filterx_json_object_new_sub(struct json_object *jso, FilterXObject *root)
{
  FilterXJsonObject *self = g_new0(FilterXJsonObject, 1);
  filterx_dict_init_instance(&self->super, &FILTERX_TYPE_NAME(json_object));

  self->super.get_subscript = _get_subscript;
  self->super.set_subscript = _set_subscript;
  self->super.unset_key = _unset_key;
  self->super.len = _len;
  self->super.iter = _iter;

  filterx_weakref_set(&self->root_container, root);
  filterx_object_unref(root);
  self->jso = jso;

  return &self->super.super;
}

static void
_free(FilterXObject *s)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  json_object_put(self->jso);
  filterx_weakref_clear(&self->root_container);
}

FilterXObject *
filterx_json_object_new_from_repr(const gchar *repr, gssize repr_len)
{
  struct json_tokener *tokener = json_tokener_new();
  struct json_object *jso;

  jso = json_tokener_parse_ex(tokener, repr, repr_len < 0 ? strlen(repr) : repr_len);
  if (repr_len >= 0 && json_tokener_get_error(tokener) == json_tokener_continue)
    {
      /* pass the closing NUL character */
      jso = json_tokener_parse_ex(tokener, "", 1);
    }

  json_tokener_free(tokener);
  return jso ? filterx_json_object_new_sub(jso, NULL) : NULL;
}

FilterXObject *
filterx_json_object_new_empty(void)
{
  return filterx_json_object_new_sub(json_object_new_object(), NULL);
}

const gchar *
filterx_json_object_to_json_literal(FilterXObject *s)
{
  FilterXJsonObject *self = (FilterXJsonObject *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(json_object)))
    return NULL;
  return json_object_to_json_string_ext(self->jso, JSON_C_TO_STRING_PLAIN);
}

FILTERX_DEFINE_TYPE(json_object, FILTERX_TYPE_NAME(dict),
                    .is_mutable = TRUE,
                    .truthy = _truthy,
                    .free_fn = _free,
                    .marshal = _marshal,
                    .repr = _repr,
                    .map_to_json = _map_to_json,
                    .clone = _clone,
                    .list_factory = filterx_json_array_new_empty,
                    .dict_factory = filterx_json_object_new_empty,
                   );
