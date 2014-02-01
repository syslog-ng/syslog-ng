/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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

#include "json-presented-persistable-state.h"
#include <stdint.h>
#include <json.h>

struct _JsonPresentedPersistableState
{
  PresentedPersistableState super;
  struct json_object *container;
};

gboolean
json_presented_persistable_state_parse_json_string(JsonPresentedPersistableState *self, gchar *json_string)
{
  struct json_object *new_container = json_tokener_parse(json_string);
  if (!new_container)
    {
      return FALSE;
    }
  json_object_put(self->container);
  self->container = new_container;
  return TRUE;
}

void
json_presented_persistable_state_add_string(PresentedPersistableState *s, gchar *name,
    gchar *value)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_object_add(self->container, name, json_object_new_string(value));
}

void
json_presented_persistable_state_add_boolean(PresentedPersistableState *s, gchar *name,
    gboolean value)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_object_add(self->container, name, json_object_new_boolean(value));
}

void
json_presented_persistable_state_add_int(PresentedPersistableState *s, gchar *name, gint value)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_object_add(self->container, name, json_object_new_int(value));
}

void
json_presented_persistable_state_add_int64(PresentedPersistableState *s, gchar *name,
    gint64 value)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_object_add(self->container, name, json_object_new_int64(value));
}

gboolean
json_presented_persistable_state_does_name_exist(PresentedPersistableState *s, gchar *name)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  return !!(value);
}

const gchar *
json_presented_persistable_state_get_string(PresentedPersistableState *s, gchar *name)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  const gchar *result = NULL;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_string)))
    {
      result = json_object_get_string(value);
    }
  return result;
}

gboolean
json_presented_persistable_state_get_boolean(PresentedPersistableState *s, gchar *name)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  gboolean result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_boolean)))
    {
      result = json_object_get_boolean(value);
    }
  return result;
}

gint
json_presented_persistable_state_get_int(PresentedPersistableState *s, gchar *name)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  gint result = -1;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int(value);
    }
  return result;
}

gint64
json_presented_persistable_state_get_int64(PresentedPersistableState *s, gchar *name)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  gint64 result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int64(value);
    }
  return result;
}

void
json_presented_persistable_state_foreach(PresentedPersistableState *s, void
    (callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_object_foreach(self->container, key, val)
    {
      callback(key, json_object_get_string(val), user_data);
    }
  return;
}

gchar *
json_presented_persistable_state_get_json_string(JsonPresentedPersistableState *self)
{
  return g_strdup(json_object_to_json_string(self->container));
}

void
json_presented_persistable_state_free(PresentedPersistableState *s)
{
  JsonPresentedPersistableState *self = (JsonPresentedPersistableState*) s;
  json_object_put(self->container);
  g_free(self);
}

PresentedPersistableState *
json_presented_persistable_state_new()
{
  JsonPresentedPersistableState *self = g_new0(JsonPresentedPersistableState, 1);
  self->container = json_object_new_object();
  self->super.add_string = json_presented_persistable_state_add_string;
  self->super.add_int64 = json_presented_persistable_state_add_int64;
  self->super.add_int = json_presented_persistable_state_add_int;
  self->super.add_boolean = json_presented_persistable_state_add_boolean;
  self->super.get_boolean = json_presented_persistable_state_get_boolean;
  self->super.get_int = json_presented_persistable_state_get_int;
  self->super.get_int64 = json_presented_persistable_state_get_int64;
  self->super.get_string = json_presented_persistable_state_get_string;
  self->super.foreach = json_presented_persistable_state_foreach;
  self->super.does_name_exist = json_presented_persistable_state_does_name_exist;
  self->super.free = json_presented_persistable_state_free;

  return &self->super;
}
