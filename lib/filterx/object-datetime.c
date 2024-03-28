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
#include "object-datetime.h"
#include "scratch-buffers.h"
#include "str-format.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/conv.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "generic-number.h"
#include "timeutils/misc.h"

#include "compat/json.h"

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

UnixTime
filterx_datetime_get_value(FilterXObject *s)
{
  FilterXDateTime *self = (FilterXDateTime *) s;

  return self->ut;
}


FilterXObject *
filterx_typecast_datetime(GPtrArray *args)
{
  if (!args || args->len == 0)
    return NULL;

  FilterXObject *object = g_ptr_array_index(args, 0);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(datetime)))
    {
      filterx_object_ref(object);
      return object;
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(integer)))
    {
      GenericNumber gn = filterx_primitive_get_value(object);
      int64_t val = gn_as_int64(&gn);
      UnixTime ut = unix_time_from_unix_epoch(val);
      return filterx_datetime_new(&ut);
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(double)))
    {
      GenericNumber gn = filterx_primitive_get_value(object);
      double val = gn_as_double(&gn);
      UnixTime ut = unix_time_from_unix_epoch((gint64)(val * USEC_PER_SEC));
      return filterx_datetime_new(&ut);
    }
  else if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      return filterx_typecast_datetime_rfc3339(args);
    }
  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "datetime"));
  return NULL;
}

FilterXObject *
filterx_typecast_datetime_rfc3339(GPtrArray *args)
{
  if (!args || args->len == 0)
    return NULL;

  FilterXObject *object = g_ptr_array_index(args, 0);

  if (!filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    return NULL;

  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  gsize len;
  const gchar *timestr = filterx_string_get_value(object, &len);
  if (len == 0)
    {
      msg_error("filterx: empty time string",
                evt_tag_str("from", object->type->name),
                evt_tag_str("to", "datetime"),
                evt_tag_str("format", "rfc3339"));
      return NULL;
    }

  gchar *end = wall_clock_time_strptime(&wct, timefmt_rfc3339, timestr);
  if (end && *end != 0)
    {
      msg_error("filterx: unable to parse time",
                evt_tag_str("from", object->type->name),
                evt_tag_str("to", "datetime"),
                evt_tag_str("format", "rfc3339"),
                evt_tag_str("time_string", timestr),
                evt_tag_str("end", end));
      return NULL;
    }

  convert_wall_clock_time_to_unix_time(&wct, &ut);
  return filterx_datetime_new(&ut);
}
