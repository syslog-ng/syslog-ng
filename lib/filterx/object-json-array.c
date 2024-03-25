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
#include "filterx/object-list-interface.h"

#include "scanner/list-scanner/list-scanner.h"
#include "str-repr/encode.h"

#define JSON_ARRAY_MAX_SIZE 65536

struct FilterXJsonArray_
{
  FilterXList super;
  FilterXWeakRef root_container;
  struct json_object *object;
};

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal_to_json_literal(FilterXJsonArray *self, GString *repr, LogMessageValueType *t)
{
  g_string_truncate(repr, 0);
  *t = LM_VT_JSON;

  const gchar *json_repr = json_object_to_json_string_ext(self->object, JSON_C_TO_STRING_PLAIN);
  g_string_append(repr, json_repr);

  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  for (gint i = 0; i < json_object_array_length(self->object); i++)
    {
      struct json_object *el = json_object_array_get_idx(self->object, i);
      if (json_object_get_type(el) != json_type_string)
        return _marshal_to_json_literal(self, repr, t);

      if (i != 0)
        g_string_append_c(repr, ',');

      str_repr_encode_append(repr, json_object_get_string(el), -1, NULL);
    }

  *t = LM_VT_LIST;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  *object = json_object_get(self->object);
  return TRUE;
}

static FilterXObject *
_clone(FilterXObject *s)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  struct json_object *json_obj = filterx_json_deep_copy(self->object);
  if (!json_obj)
    return NULL;

  return filterx_json_array_new_sub(json_obj, NULL);
}

static FilterXObject *
_get_subscript(FilterXList *s, guint64 index)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  struct json_object *result = json_object_array_get_idx(self->object, index);
  if (!result)
    return NULL;

  return filterx_json_convert_json_to_object_cached(&s->super, &self->root_container, result);
}

static gboolean
_append(FilterXList *s, FilterXObject *new_value)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  if (G_UNLIKELY(filterx_list_len(&s->super) >= JSON_ARRAY_MAX_SIZE))
    return FALSE;

  struct json_object *new_json_value = NULL;
  if (!filterx_object_map_to_json(new_value, &new_json_value))
    return FALSE;

  if (json_object_array_add(self->object, new_json_value) != 0)
    {
      json_object_put(new_json_value);
      return FALSE;
    }

  self->super.super.modified_in_place = TRUE;
  FilterXObject *root_container = filterx_weakref_get(&self->root_container);
  if (root_container)
    {
      root_container->modified_in_place = TRUE;
      filterx_object_unref(root_container);
    }

  return TRUE;
}

static gboolean
_set_subscript(FilterXList *s, guint64 index, FilterXObject *new_value)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  if (G_UNLIKELY(index >= JSON_ARRAY_MAX_SIZE))
    return FALSE;

  struct json_object *new_json_value = NULL;
  if (!filterx_object_map_to_json(new_value, &new_json_value))
    return FALSE;

  filterx_json_associate_cached_object(new_json_value, new_value);

  if (json_object_array_put_idx(self->object, index, new_json_value) != 0)
    {
      json_object_put(new_json_value);
      return FALSE;
    }

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
_len(FilterXList *s)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  return json_object_array_length(self->object);
}

/* NOTE: consumes root ref */
FilterXObject *
filterx_json_array_new_sub(struct json_object *object, FilterXObject *root)
{
  FilterXJsonArray *self = g_new0(FilterXJsonArray, 1);
  filterx_list_init_instance(&self->super, &FILTERX_TYPE_NAME(json_array));

  self->super.get_subscript = _get_subscript;
  self->super.set_subscript = _set_subscript;
  self->super.append = _append;
  self->super.len = _len;

  filterx_weakref_set(&self->root_container, root);
  filterx_object_unref(root);
  self->object = object;

  return &self->super.super;
}

static void
_free(FilterXObject *s)
{
  FilterXJsonArray *self = (FilterXJsonArray *) s;

  json_object_put(self->object);
  filterx_weakref_clear(&self->root_container);
}

FilterXObject *
filterx_json_array_new_from_repr(const gchar *repr, gssize repr_len)
{
  struct json_object *object = json_object_new_array();

  ListScanner scanner;
  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, repr, repr_len);
  for (gint i = 0; list_scanner_scan_next(&scanner); i++)
    {
      json_object_array_put_idx(object, i,
                                json_object_new_string_len(list_scanner_get_current_value(&scanner),
                                                           list_scanner_get_current_value_len(&scanner)));
    }
  list_scanner_deinit(&scanner);

  return filterx_json_array_new_sub(object, NULL);
}

FilterXObject *
filterx_json_array_new_empty(void)
{
  return filterx_json_array_new_sub(json_object_new_array(), NULL);
}

FILTERX_DEFINE_TYPE(json_array, FILTERX_TYPE_NAME(list),
                    .is_mutable = TRUE,
                    .truthy = _truthy,
                    .free_fn = _free,
                    .marshal = _marshal,
                    .map_to_json = _map_to_json,
                    .clone = _clone,
                   );
