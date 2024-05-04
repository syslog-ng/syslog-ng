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
#include "filterx/object-primitive.h"
#include "filterx/filterx-grammar.h"
#include "filterx/object-string.h"
#include "generic-number.h"
#include "str-format.h"
#include "plugin.h"
#include "cfg.h"
#include "filterx-globals.h"

#include "compat/json.h"

#define FILTERX_BOOL_CACHE_LIMIT 2
#define FILTERX_INTEGER_CACHE_LIMIT 100

static FilterXObject *bool_cache[FILTERX_BOOL_CACHE_LIMIT];
static FilterXObject *integer_cache[FILTERX_INTEGER_CACHE_LIMIT];

static gboolean
_truthy(FilterXObject *s)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  return !gn_is_zero(&self->value);
}

static FilterXPrimitive *
filterx_primitive_new(FilterXType *type)
{
  FilterXPrimitive *self = g_new0(FilterXPrimitive, 1);

  filterx_object_init_instance(&self->super, type);
  return self;
}

static gboolean
_integer_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_int64(gn_as_int64(&self->value));
  return TRUE;
}

gboolean
integer_repr(gint64 val, GString *repr)
{
  format_int64_padded(repr, 0, 0, 10, val);
  return TRUE;
}

static gboolean
_integer_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;
  *t = LM_VT_INTEGER;
  return integer_repr(gn_as_int64(&self->value), repr);
}

static inline FilterXObject *
_integer_wrap(gint64 value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(integer));
  gn_set_int64(&self->value, value);
  return &self->super;
}

FilterXObject *
filterx_integer_new(gint64 value)
{
  if (value >= -1 && value < FILTERX_INTEGER_CACHE_LIMIT - 1)
    return filterx_object_ref(integer_cache[value + 1]);
  return _integer_wrap(value);
}

static gboolean
_double_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_double(gn_as_double(&self->value));
  return TRUE;
}

gboolean
double_repr(double val, GString *repr)
{
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
  g_ascii_dtostr(buf, sizeof(buf), val);
  g_string_append(repr, buf);
  return TRUE;
}

static gboolean
_double_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;
  *t = LM_VT_DOUBLE;
  return double_repr(gn_as_double(&self->value), repr);
}

FilterXObject *
filterx_double_new(gdouble value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(double));
  gn_set_double(&self->value, value, -1);
  return &self->super;
}

gboolean
bool_repr(gboolean bool_val, GString *repr)
{
  if (bool_val)
    g_string_append(repr, "true");
  else
    g_string_append(repr, "false");
  return TRUE;
}

static gboolean
_bool_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;
  *t = LM_VT_BOOLEAN;
  return bool_repr(!gn_is_zero(&self->value), repr);
}

static gboolean
_bool_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_boolean(gn_as_int64(&self->value));
  return TRUE;
}

FilterXObject *
_bool_wrap(gboolean value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(boolean));
  gn_set_int64(&self->value, (gint64) value);
  return &self->super;
}

FilterXObject *
filterx_boolean_new(gboolean value)
{
  return filterx_object_ref(bool_cache[!!(value)]);
}

static gboolean
_lookup_enum_value_from_array(const FilterXEnumDefinition *enum_defs, const gchar *enum_name, gint64 *value)
{
  gint64 i = 0;
  while (TRUE)
    {
      const FilterXEnumDefinition *enum_def = &enum_defs[i];

      if (!enum_def->name)
        return FALSE;

      if (strcasecmp(enum_def->name, enum_name) != 0)
        {
          i++;
          continue;
        }

      *value = enum_def->value;
      return TRUE;
    }
}

static gboolean
_lookup_enum_value(GlobalConfig *cfg, const gchar *namespace_name, const gchar *enum_name, gint64 *value)
{
  Plugin *p = cfg_find_plugin(cfg, LL_CONTEXT_FILTERX_ENUM, namespace_name);

  if (p == NULL)
    return FALSE;

  const FilterXEnumDefinition *enum_defs = plugin_construct(p);
  if (enum_defs == NULL)
    return FALSE;

  return _lookup_enum_value_from_array(enum_defs, enum_name, value);
}

FilterXObject *
filterx_enum_new(GlobalConfig *cfg, const gchar *namespace_name, const gchar *enum_name)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(integer));

  gint64 value;
  if (!_lookup_enum_value(cfg, namespace_name, enum_name, &value))
    {
      filterx_object_unref(&self->super);
      return NULL;
    }

  gn_set_int64(&self->value, value);
  return &self->super;
}

GenericNumber
filterx_primitive_get_value(FilterXObject *s)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;
  return self->value;
}

FilterXObject *
filterx_typecast_boolean(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(boolean)))
    {
      filterx_object_ref(object);
      return object;
    }

  return filterx_boolean_new(filterx_object_truthy(object));
}

FilterXObject *
filterx_typecast_integer(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
    {
      filterx_object_ref(object);
      return object;
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
    {
      GenericNumber gn = filterx_primitive_get_value(object);
      return filterx_integer_new(gn_as_int64(&gn));
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      gsize size;
      gchar *endptr;
      const gchar *str = filterx_string_get_value(object, &size);

      gint64 val = g_ascii_strtoll(str, &endptr, 10);
      if (str != endptr && *endptr == '\0')
        return filterx_integer_new(val);
    }

  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "integer"));
  return NULL;
}

FilterXObject *
filterx_typecast_double(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
    {
      filterx_object_ref(object);
      return object;
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
    {
      GenericNumber gn = filterx_primitive_get_value(object);
      return filterx_double_new(gn_as_double(&gn));
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      gsize size;
      gchar *endptr;
      const gchar *str = filterx_string_get_value(object, &size);

      gdouble val = g_ascii_strtod(str, &endptr);
      if (str != endptr && *endptr == '\0')
        return filterx_double_new(val);
    }

  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "double"));
  return NULL;
}

static gboolean
_repr(FilterXObject *s, GString *repr)
{
  LogMessageValueType t;
  return filterx_object_marshal(s, repr, &t);
}

FILTERX_DEFINE_TYPE(integer, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _integer_marshal,
                    .map_to_json = _integer_map_to_json,
                    .repr = _repr,
                   );

FILTERX_DEFINE_TYPE(double, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _double_marshal,
                    .map_to_json = _double_map_to_json,
                    .repr = _repr,
                   );

FILTERX_DEFINE_TYPE(boolean, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _bool_marshal,
                    .map_to_json = _bool_map_to_json,
                    .repr = _repr,
                   );

void
filterx_primitive_global_init(void)
{
  filterx_cache_object(&bool_cache[FALSE], _bool_wrap(FALSE));
  filterx_cache_object(&bool_cache[TRUE], _bool_wrap(TRUE));
  for (guint64 i = 0; i < FILTERX_INTEGER_CACHE_LIMIT; i++)
    filterx_cache_object(&integer_cache[i], _integer_wrap(i - 1));
}

void
filterx_primitive_global_deinit(void)
{
  filterx_uncache_object(&bool_cache[FALSE]);
  filterx_uncache_object(&bool_cache[TRUE]);
  for (guint64 i = 0; i < FILTERX_INTEGER_CACHE_LIMIT; i++)
    filterx_uncache_object(&integer_cache[i]);
}
