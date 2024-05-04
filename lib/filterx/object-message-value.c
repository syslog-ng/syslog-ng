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
#include "filterx/object-message-value.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-datetime.h"
#include "filterx/object-json.h"
#include "logmsg/type-hinting.h"
#include "str-utils.h"

/* an object representing a (type, value) pair extracted as an rvalue (e.g.
 * cannot be assigned to as it is not part of the message) */
typedef struct _FilterXMessageValue
{
  FilterXObject super;
  const gchar *repr;
  gsize repr_len;
  LogMessageValueType type;
  gchar *buf;
} FilterXMessageValue;

gboolean
_is_value_type_pair_truthy(const gchar  *repr, gssize repr_len, LogMessageValueType type)
{
  gboolean b;
  gdouble d;
  gint64 i64;

  switch (type)
    {
    case LM_VT_BOOLEAN:
      if (type_cast_to_boolean(repr, repr_len, &b, NULL) && b)
        return TRUE;
      break;
    case LM_VT_INTEGER:
      if (type_cast_to_int64(repr, repr_len, &i64, NULL) && i64)
        return TRUE;
      break;
    case LM_VT_DOUBLE:
      if (type_cast_to_double(repr, repr_len, &d, NULL) && d < DBL_EPSILON)
        return TRUE;
      break;
    case LM_VT_STRING:
      if (repr_len > 0)
        return TRUE;
      break;
    case LM_VT_JSON:
    case LM_VT_LIST:
    case LM_VT_DATETIME:
      return TRUE;
    default:
      break;
    }
  return FALSE;
}

FilterXObject *
_unmarshal_repr(const gchar *repr, gssize repr_len, LogMessageValueType t)
{
  gdouble dbl;
  gint64 i64;
  gboolean b;
  UnixTime ut;

  switch (t)
    {
    case LM_VT_STRING:
      return filterx_string_new(repr, repr_len);
    case LM_VT_JSON:
      return filterx_json_new_from_repr(repr, repr_len);
    case LM_VT_BOOLEAN:
      if (!type_cast_to_boolean(repr, repr_len, &b, NULL))
        return NULL;
      return filterx_boolean_new(b);
    case LM_VT_INTEGER:
      if (!type_cast_to_int64(repr, repr_len, &i64, NULL))
        return NULL;
      return filterx_integer_new(i64);
    case LM_VT_DOUBLE:
      if (!type_cast_to_double(repr, repr_len, &dbl, NULL))
        return NULL;
      return filterx_double_new(dbl);
    case LM_VT_DATETIME:
      if (!type_cast_to_datetime_unixtime(repr, repr_len, &ut, NULL))
        return NULL;
      return filterx_datetime_new(&ut);
    case LM_VT_LIST:
      return filterx_json_array_new_from_syslog_ng_list(repr, repr_len);
    case LM_VT_NULL:
      return filterx_null_new();
    case LM_VT_BYTES:
      return filterx_bytes_new(repr, repr_len);
    case LM_VT_PROTOBUF:
      return filterx_protobuf_new(repr, repr_len);
    default:
      g_assert_not_reached();
    }
  return NULL;
}

/* NOTE: calling map_to_json() on a FilterXMessageBase is less than ideal as
 * we would unmarshal the value and then drop the result.  The expectation
 * is that the caller would explicitly unmarshall first, cache the result
 * and call map_to_json on the unmarshalled object.
 */
static gboolean
_map_to_json(FilterXObject *s, struct json_object **jso, FilterXObject **assoc_object)
{
  FilterXObject *unmarshalled_object = filterx_object_unmarshal(filterx_object_ref(s));

  if (unmarshalled_object)
    {
      gboolean result = filterx_object_map_to_json(unmarshalled_object, jso, assoc_object);
      filterx_object_unref(unmarshalled_object);
      return result;
    }
  else
    return FALSE;
}

static gboolean
_truthy(FilterXObject *s)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  return _is_value_type_pair_truthy(self->repr, self->repr_len, self->type);
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  g_string_append_len(repr, self->repr, self->repr_len);
  *t = self->type;
  return TRUE;
}

static FilterXObject *
_unmarshal(FilterXObject *s)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;
  return _unmarshal_repr(self->repr, self->repr_len, self->type);
}

static gboolean
_len(FilterXObject *s, guint64 *len)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  switch (self->type)
    {
    case LM_VT_STRING:
    case LM_VT_BYTES:
    case LM_VT_PROTOBUF:
      *len = self->repr_len;
      return TRUE;
    case LM_VT_JSON:
    case LM_VT_LIST:
    {
      /* These get lost here, but we cannot do better without knowing the handle. */
      FilterXObject *unmarshaled = filterx_object_unmarshal(s);
      gboolean result = filterx_object_len(unmarshaled, len);
      filterx_object_unref(unmarshaled);
      return result;
    }
    case LM_VT_BOOLEAN:
    case LM_VT_INTEGER:
    case LM_VT_DOUBLE:
    case LM_VT_DATETIME:
    case LM_VT_NULL:
      return FALSE;
    default:
      g_assert_not_reached();
    }
  return FALSE;
}

LogMessageValueType
filterx_message_value_get_type(FilterXObject *s)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;
  return self->type;
}

const gchar *
filterx_message_value_get_value(FilterXObject *s, gsize *len)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  g_assert(len);
  *len = self->repr_len;
  return self->repr;
}

/* NOTE: the caller must ensure that repr lives as long as the constructed object, avoids copying */
FilterXObject *
filterx_message_value_new_borrowed(const gchar *repr, gssize repr_len, LogMessageValueType type)
{
  FilterXMessageValue *self = g_new0(FilterXMessageValue, 1);

  filterx_object_init_instance(&self->super, &FILTERX_TYPE_NAME(message_value));
  self->repr = repr;
  self->repr_len = repr_len < 0 ? strlen(repr) : repr_len;
  self->type = type;
  return &self->super;
}

/* NOTE: copies repr */
FilterXObject *
filterx_message_value_new(const gchar *repr, gssize repr_len, LogMessageValueType type)
{
  gssize len = repr_len < 0 ? strlen(repr) : repr_len;
  gssize alloc_len = repr_len < 0 ? len + 1 : repr_len;
  gchar *buf = g_memdup2(repr, alloc_len);
  FilterXMessageValue *self = (FilterXMessageValue *) filterx_message_value_new_borrowed(buf, len, type);
  self->buf = buf;
  return &self->super;
}

/* NOTE: takes over the responsibility of freeing repr */
FilterXObject *
filterx_message_value_new_ref(gchar *repr, gssize repr_len, LogMessageValueType type)
{
  FilterXMessageValue *self = (FilterXMessageValue *) filterx_message_value_new_borrowed(repr, repr_len, type);
  self->buf = repr;
  return &self->super;
}

static void
_free(FilterXObject *s)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  g_free(self->buf);
}

static gboolean
_repr(FilterXObject *s, GString *repr)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;

  switch (self->type)
    {
    case LM_VT_STRING:
      g_string_append_len(repr, self->repr, self->repr_len);
      return TRUE;
    case LM_VT_JSON:
      g_string_append_len(repr, self->repr, self->repr_len);
      return TRUE;
    case LM_VT_BOOLEAN:
    {
      gboolean val;
      if (!type_cast_to_boolean(self->repr, self->repr_len, &val, NULL))
        return FALSE;
      return bool_repr(val, repr);
    }
    case LM_VT_INTEGER:
    {
      gint64 val;
      if (!type_cast_to_int64(self->repr, self->repr_len, &val, NULL))
        return FALSE;
      return integer_repr(val, repr);
    }
    case LM_VT_DOUBLE:
    {
      double val;
      if (!type_cast_to_double(self->repr, self->repr_len, &val, NULL))
        return FALSE;
      return double_repr(val, repr);
    }
    case LM_VT_DATETIME:
    {
      UnixTime ut = UNIX_TIME_INIT;
      if (!type_cast_to_datetime_unixtime(self->repr, self->repr_len, &ut, NULL))
        return FALSE;
      return datetime_repr(&ut, repr);
    }
    case LM_VT_LIST:
    {
      FilterXObject *obj = filterx_object_unmarshal(s);
      filterx_object_repr(obj, repr);
      filterx_object_unref(obj);
      return TRUE;
    }
    case LM_VT_NULL:
      return null_repr(repr);
    case LM_VT_BYTES:
      g_string_append_len(repr, self->repr, self->repr_len);
      return TRUE;
    case LM_VT_PROTOBUF:
      g_string_append_len(repr, self->repr, self->repr_len);
      return TRUE;
    default:
      g_assert_not_reached();
    }

  return FALSE;
}

FILTERX_DEFINE_TYPE(message_value, FILTERX_TYPE_NAME(object),
                    .free_fn = _free,
                    .truthy = _truthy,
                    .marshal = _marshal,
                    .unmarshal = _unmarshal,
                    .len = _len,
                    .map_to_json = _map_to_json,
                    .repr = _repr,
                   );
