#include "object-datetime.h"
#include "scratch-buffers.h"
#include "str-format.h"

#include <json-c/json.h>


typedef struct _FilterXDateTime
{
  FilterXObject super;
  UnixTime ut;
} FilterXDateTime;

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

/* FIXME: delegate formatting and parsing to UnixTime */
static void
_convert_unix_time_to_string(const UnixTime *ut, GString *result)
{

  format_int64_padded(result, -1, ' ', 10, ut->ut_sec);
  g_string_append_c(result, '.');
  format_uint64_padded(result, 6, '0', 10, ut->ut_usec);
  if (ut->ut_gmtoff != -1)
    {
      if (ut->ut_gmtoff >= 0)
        g_string_append_c(result, '+');
      else
        g_string_append_c(result, '-');

      format_int64_padded(result, 2, '0', 10, ut->ut_gmtoff / 3600);
      g_string_append_c(result, ':');
      format_int64_padded(result, 2, '0', 10, (ut->ut_gmtoff % 3600) / 60);
    }
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXDateTime *self = (FilterXDateTime *) s;

  *t = LM_VT_DATETIME;
  g_string_truncate(repr, 0);
  _convert_unix_time_to_string(&self->ut, repr);
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXDateTime *self = (FilterXDateTime *) s;
  GString *time_stamp = scratch_buffers_alloc();

  _convert_unix_time_to_string(&self->ut, time_stamp);

  *object = json_object_new_string_len(time_stamp->str, time_stamp->len);
  return TRUE;
}

FilterXObject *
filterx_datetime_new(const UnixTime *ut)
{
  FilterXDateTime *self = g_new0(FilterXDateTime, 1);

  filterx_object_init_instance(&self->super, &FILTERX_TYPE_NAME(datetime));
  self->ut = *ut;
  return &self->super;
}

FILTERX_DEFINE_TYPE(datetime, FILTERX_TYPE_NAME(object),
  .truthy = _truthy,
  .map_to_json = _map_to_json,
  .marshal = _marshal,
);
