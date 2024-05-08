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

#include "filterx/object-dict-interface.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"

gboolean
filterx_dict_iter(FilterXObject *s, FilterXDictIterFunc func, gpointer user_data)
{
  FilterXDict *self = (FilterXDict *) s;
  if (!self->iter)
    return FALSE;
  return self->iter(self, func, user_data);
}

static gboolean
_add_elem_to_dict(FilterXObject *key_obj, FilterXObject *value_obj, gpointer user_data)
{
  FilterXObject *dict = (FilterXObject *) user_data;

  FilterXObject *new_value = filterx_object_ref(value_obj);
  gboolean success = filterx_object_set_subscript(dict, key_obj, &new_value);
  filterx_object_unref(new_value);

  return success;
}

gboolean
filterx_dict_merge(FilterXObject *s, FilterXObject *other)
{
  FilterXDict *self = (FilterXDict *) s;

  g_assert(filterx_object_is_type(other, &FILTERX_TYPE_NAME(dict)));
  return filterx_dict_iter(other, _add_elem_to_dict, self);
}

static gboolean
_len(FilterXObject *s, guint64 *len)
{
  FilterXDict *self = (FilterXDict *) s;
  *len = self->len(self);
  return TRUE;
}

static FilterXObject *
_get_subscript(FilterXObject *s, FilterXObject *key)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!key)
    {
      msg_error("FilterX: Failed to get element of dict, key is mandatory");
      return NULL;
    }

  return self->get_subscript(self, key);
}

static gboolean
_set_subscript(FilterXObject *s, FilterXObject *key, FilterXObject **new_value)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!key)
    {
      msg_error("FilterX: Failed to set element of dict, key is mandatory");
      return FALSE;
    }

  return self->set_subscript(self, key, new_value);
}

static gboolean
_is_key_set(FilterXObject *s, FilterXObject *key)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!key)
    {
      msg_error("FilterX: Failed to check key of dict, key is mandatory");
      return FALSE;
    }

  if (self->is_key_set)
    return self->is_key_set(self, key);

  FilterXObject *value = self->get_subscript(self, key);
  filterx_object_unref(value);
  return !!value;
}

static gboolean
_unset_key(FilterXObject *s, FilterXObject *key)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!key)
    {
      msg_error("FilterX: Failed to unset element of dict, key is mandatory");
      return FALSE;
    }

  return self->unset_key(self, key);
}

static FilterXObject *
_getattr(FilterXObject *s, FilterXObject *attr)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!self->support_attr)
    return NULL;

  FilterXObject *result = self->get_subscript(self, attr);
  return result;
}

static gboolean
_setattr(FilterXObject *s, FilterXObject *attr, FilterXObject **new_value)
{
  FilterXDict *self = (FilterXDict *) s;

  if (!self->support_attr)
    return FALSE;

  gboolean result = self->set_subscript(self, attr, new_value);
  return result;
}

static gboolean
_add_elem_to_json_object(FilterXObject *key_obj, FilterXObject *value_obj, gpointer user_data)
{
  struct json_object *object = (struct json_object *) user_data;

  /* FilterX strings are always NUL terminated. */
  const gchar *key = filterx_string_get_value(key_obj, NULL);
  if (!key)
    return FALSE;

  struct json_object *value = NULL;
  FilterXObject *assoc_object = NULL;
  if (!filterx_object_map_to_json(value_obj, &value, &assoc_object))
    return FALSE;

  filterx_json_associate_cached_object(object, assoc_object);
  filterx_object_unref(assoc_object);

  if (json_object_object_add(object, key, value) != 0)
    {
      json_object_put(value);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  *object = json_object_new_object();
  if (!filterx_dict_iter(s, _add_elem_to_json_object, *object))
    {
      json_object_put(*object);
      *object = NULL;
      return FALSE;
    }

  *assoc_object = filterx_json_new_from_object(json_object_get(*object));
  return TRUE;
}

void
filterx_dict_init_instance(FilterXDict *self, FilterXType *type)
{
  g_assert(type->is_mutable);
  g_assert(type->len == _len);
  g_assert(type->get_subscript == _get_subscript);
  g_assert(type->set_subscript == _set_subscript);
  g_assert(type->is_key_set == _is_key_set);
  g_assert(type->unset_key == _unset_key);
  g_assert(type->getattr == _getattr);
  g_assert(type->setattr == _setattr);

  filterx_object_init_instance(&self->super, type);

  self->support_attr = TRUE;
}

FILTERX_DEFINE_TYPE(dict, FILTERX_TYPE_NAME(object),
                    .is_mutable = TRUE,
                    .len = _len,
                    .get_subscript = _get_subscript,
                    .set_subscript = _set_subscript,
                    .is_key_set = _is_key_set,
                    .unset_key = _unset_key,
                    .getattr = _getattr,
                    .setattr = _setattr,
                    .map_to_json = _map_to_json,
                   );
