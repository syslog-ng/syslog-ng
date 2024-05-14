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
#ifndef FILTERX_OBJECT_H_INCLUDED
#define FILTERX_OBJECT_H_INCLUDED

#include "logmsg/logmsg.h"
#include "compat/json.h"

typedef struct _FilterXType FilterXType;
typedef struct _FilterXObject FilterXObject;

struct _FilterXType
{
  FilterXType *super_type;
  const gchar *name;
  gboolean is_mutable;

  FilterXObject *(*unmarshal)(FilterXObject *self);
  gboolean (*marshal)(FilterXObject *self, GString *repr, LogMessageValueType *t);
  FilterXObject *(*clone)(FilterXObject *self);
  gboolean (*map_to_json)(FilterXObject *self, struct json_object **object, FilterXObject **assoc_object);
  gboolean (*truthy)(FilterXObject *self);
  FilterXObject *(*getattr)(FilterXObject *self, FilterXObject *attr);
  gboolean (*setattr)(FilterXObject *self, FilterXObject *attr, FilterXObject **new_value);
  FilterXObject *(*get_subscript)(FilterXObject *self, FilterXObject *key);
  gboolean (*set_subscript)(FilterXObject *self, FilterXObject *key, FilterXObject **new_value);
  gboolean (*is_key_set)(FilterXObject *self, FilterXObject *key);
  gboolean (*unset_key)(FilterXObject *self, FilterXObject *key);
  FilterXObject *(*list_factory)(void);
  FilterXObject *(*dict_factory)(void);
  gboolean (*repr)(FilterXObject *self, GString *repr);
  gboolean (*len)(FilterXObject *self, guint64 *len);
  void (*free_fn)(FilterXObject *self);
};

void filterx_type_init(FilterXType *type);

#define FILTERX_TYPE_NAME(_name) filterx_type_ ## _name
#define FILTERX_DECLARE_TYPE(_name) \
    extern FilterXType FILTERX_TYPE_NAME(_name)
#define FILTERX_DEFINE_TYPE(_name, _super_type, ...) \
    FilterXType FILTERX_TYPE_NAME(_name) =  \
    {           \
      .super_type = &_super_type,   \
      .name = # _name,        \
      __VA_ARGS__       \
    }


FILTERX_DECLARE_TYPE(object);

struct _FilterXObject
{
  /* NOTE: this packs into 16 bytes in total (64 bit), let's try to keep
   * this small, potentially using bitfields.  A simple boolean is 32 bytes
   * in total at the moment (factoring in GenericNumber which is used to
   * represent it).  Maybe we could get rid off the GenericNumber wrapper
   * which would potentially decrease the struct to 16-24 bytes. */

  gint ref_cnt;

  /* NOTE:
   *
   *     modified_in_place -- set to TRUE in case the value in this
   *                          FilterXObject was changed
   *     readonly          -- marks the object as unmodifiable,
   *                          propagates to the inner elements lazily
   *
   */
  guint thread_index:16, modified_in_place:1, readonly:1, weak_referenced:1;
  FilterXType *type;
};

FilterXObject *filterx_object_getattr_string(FilterXObject *self, const gchar *attr_name);
gboolean filterx_object_setattr_string(FilterXObject *self, const gchar *attr_name, FilterXObject **new_value);

FilterXObject *filterx_object_new(FilterXType *type);
FilterXObject *filterx_object_ref(FilterXObject *self);
void filterx_object_unref(FilterXObject *self);
gboolean filterx_object_freeze(FilterXObject *self);
gboolean filterx_object_is_frozen(FilterXObject *self);
void filterx_object_unfreeze_and_free(FilterXObject *self);
void filterx_object_init_instance(FilterXObject *self, FilterXType *type);
void filterx_object_free_method(FilterXObject *self);

static inline gboolean
filterx_object_is_type(FilterXObject *object, FilterXType *type)
{
  FilterXType *self_type = object->type;
  while (self_type)
    {
      if (type == self_type)
        return TRUE;
      self_type = self_type->super_type;
    }
  return FALSE;
}

static inline void
filterx_object_make_readonly(FilterXObject *self)
{
  self->readonly = TRUE;
}

static inline FilterXObject *
filterx_object_unmarshal(FilterXObject *self)
{
  if (self->type->unmarshal)
    return self->type->unmarshal(self);
  return filterx_object_ref(self);
}

static inline gboolean
filterx_object_repr(FilterXObject *self, GString *repr)
{
  if (self->type->repr)
    {
      g_string_truncate(repr, 0);
      return self->type->repr(self, repr);
    }
  return FALSE;
}

static inline gboolean
filterx_object_repr_append(FilterXObject *self, GString *repr)
{
  if (self->type->repr)
    {
      return self->type->repr(self, repr);
    }
  return FALSE;
}

static inline gboolean
filterx_object_len(FilterXObject *self, guint64 *len)
{
  if (!self->type->len)
    return FALSE;
  return self->type->len(self, len);
}

static inline gboolean
filterx_object_marshal(FilterXObject *self, GString *repr, LogMessageValueType *t)
{
  if (self->type->marshal)
    {
      g_string_truncate(repr, 0);
      return self->type->marshal(self, repr, t);
    }
  return FALSE;
}

static inline gboolean
filterx_object_marshal_append(FilterXObject *self, GString *repr, LogMessageValueType *t)
{
  if (self->type->marshal)
    return self->type->marshal(self, repr, t);
  return FALSE;
}

static inline FilterXObject *
filterx_object_clone(FilterXObject *self)
{
  if (self->readonly)
    return filterx_object_ref(self);
  return self->type->clone(self);
}

static inline gboolean
filterx_object_map_to_json(FilterXObject *self, struct json_object **object, FilterXObject **assoc_object)
{
  *assoc_object = NULL;
  if (self->type->map_to_json)
    {
      gboolean result = self->type->map_to_json(self, object, assoc_object);
      if (!(*assoc_object))
        *assoc_object = filterx_object_ref(self);
      return result;
    }
  return FALSE;
}

static inline gboolean
filterx_object_truthy(FilterXObject *self)
{
  return self->type->truthy(self);
}

static inline gboolean
filterx_object_falsy(FilterXObject *self)
{
  return !filterx_object_truthy(self);
}

static inline FilterXObject *
filterx_object_getattr(FilterXObject *self, FilterXObject *attr)
{
  if (!self->type->getattr)
    return NULL;

  FilterXObject *result = self->type->getattr(self, attr);
  if (result && self->readonly)
    filterx_object_make_readonly(result);
  return result;
}

static inline gboolean
filterx_object_setattr(FilterXObject *self, FilterXObject *attr, FilterXObject **new_value)
{
  g_assert(!self->readonly);

  if (self->type->setattr)
    return self->type->setattr(self, attr, new_value);
  return FALSE;
}

static inline FilterXObject *
filterx_object_get_subscript(FilterXObject *self, FilterXObject *key)
{
  if (!self->type->get_subscript)
    return NULL;

  FilterXObject *result = self->type->get_subscript(self, key);
  if (result && self->readonly)
    filterx_object_make_readonly(result);
  return result;
}

static inline gboolean
filterx_object_set_subscript(FilterXObject *self, FilterXObject *key, FilterXObject **new_value)
{
  g_assert(!self->readonly);

  if (self->type->set_subscript)
    return self->type->set_subscript(self, key, new_value);
  return FALSE;
}

static inline gboolean
filterx_object_is_key_set(FilterXObject *self, FilterXObject *key)
{
  if (self->type->is_key_set)
    return self->type->is_key_set(self, key);
  return FALSE;
}

static inline gboolean
filterx_object_unset_key(FilterXObject *self, FilterXObject *key)
{
  g_assert(!self->readonly);

  if (self->type->unset_key)
    return self->type->unset_key(self, key);
  return FALSE;
}

static inline FilterXObject *
filterx_object_create_list(FilterXObject *self)
{
  if (!self->type->list_factory)
    return NULL;

  return self->type->list_factory();
}

static inline FilterXObject *
filterx_object_create_dict(FilterXObject *self)
{
  if (!self->type->dict_factory)
    return NULL;

  return self->type->dict_factory();
}

#endif
