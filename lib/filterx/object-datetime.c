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
