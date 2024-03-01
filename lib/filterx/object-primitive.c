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
#include "generic-number.h"
#include "str-format.h"
#include "plugin.h"
#include "cfg.h"

#include "compat/json.h"

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
_integer_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_int64(gn_as_int64(&self->value));
  return TRUE;
}

static gboolean
_integer_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  format_int64_padded(repr, 0, 0, 10, gn_as_int64(&self->value));
  *t = LM_VT_INTEGER;
  return TRUE;
}

FilterXObject *
filterx_integer_new(gint64 value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(integer));
  gn_set_int64(&self->value, value);
  return &self->super;
}

static gboolean
_double_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_double(gn_as_double(&self->value));
  return TRUE;
}

static gboolean
_double_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;
  gchar buf[32];

  g_ascii_dtostr(buf, sizeof(buf), gn_as_double(&self->value));
  g_string_append(repr, buf);
  *t = LM_VT_DOUBLE;
  return TRUE;
}

FilterXObject *
filterx_double_new(gdouble value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(double));
  gn_set_double(&self->value, value, -1);
  return &self->super;
}

static gboolean
_bool_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  if (!gn_is_zero(&self->value))
    g_string_append(repr, "true");
  else
    g_string_append(repr, "false");
  *t = LM_VT_BOOLEAN;
  return TRUE;
}

static gboolean
_bool_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  *object = json_object_new_boolean(gn_as_int64(&self->value));
  return TRUE;
}

FilterXObject *
_filterx_boolean_new(gboolean value)
{
  FilterXPrimitive *self = filterx_primitive_new(&FILTERX_TYPE_NAME(boolean));
  gn_set_int64(&self->value, (gint64) value);
  return &self->super;
}

FilterXObject *
filterx_boolean_new(gboolean value)
{
  static FilterXObject *true_object = NULL;
  static FilterXObject *false_object = NULL;

  if (value)
    {
      if (!true_object)
        {
          true_object = _filterx_boolean_new(TRUE);
          filterx_object_freeze(true_object);
        }
      return filterx_object_ref(true_object);
    }
  else
    {
      if (!false_object)
        {
          false_object = _filterx_boolean_new(FALSE);
          filterx_object_freeze(false_object);
        }
      return filterx_object_ref(false_object);
    }
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

FILTERX_DEFINE_TYPE(integer, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _integer_marshal,
                    .map_to_json = _integer_map_to_json,
                   );

FILTERX_DEFINE_TYPE(double, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _double_marshal,
                    .map_to_json = _double_map_to_json,
                   );

FILTERX_DEFINE_TYPE(boolean, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .marshal = _bool_marshal,
                    .map_to_json = _bool_map_to_json,
                   );
