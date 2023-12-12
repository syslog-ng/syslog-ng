#include "filterx/object-message-value.h"
#include "filterx/object-message-base.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-datetime.h"
#include "filterx/object-list.h"
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
      return construct_filterx_json_from_repr(repr, repr_len);
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
      return filterx_list_new(repr, repr_len);
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
_map_to_json(FilterXObject *s, struct json_object **jso)
{
  FilterXObject *unmarshalled_object = filterx_object_unmarshal(filterx_object_ref(s));

  if (unmarshalled_object)
    {
      gboolean result = filterx_object_map_to_json(unmarshalled_object, jso);
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

  g_string_assign_len(repr, self->repr, self->repr_len);
  *t = self->type;
  return TRUE;
}

static FilterXObject *
_unmarshal(FilterXObject *s)
{
  FilterXMessageValue *self = (FilterXMessageValue *) s;
  return _unmarshal_repr(self->repr, self->repr_len, self->type);
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
  gchar *buf = g_memdup(repr, repr_len);
  FilterXMessageValue *self = (FilterXMessageValue *) filterx_message_value_new_borrowed(buf, repr_len, type);
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


FILTERX_DEFINE_TYPE(message_value, FILTERX_TYPE_NAME(object),
  .free_fn = _free,
  .truthy = _truthy,
  .marshal = _marshal,
  .unmarshal = _unmarshal,
  .map_to_json = _map_to_json,
);
