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
#include "str-utils.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/conv.h"
#include "timeutils/misc.h"
#include "timeutils/format.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-message-value.h"
#include "filterx/object-null.h"
#include "filterx/expr-literal.h"
#include "generic-number.h"
#include "filterx/filterx-eval.h"
#include "filterx-globals.h"
#include "compat/json.h"

#define FILTERX_FUNC_STRPTIME_USAGE "Usage: strptime(time_str, format_str_1, ..., format_str_N)"

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
_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
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

UnixTime
filterx_datetime_get_value(FilterXObject *s)
{
  FilterXDateTime *self = (FilterXDateTime *) s;

  return self->ut;
}


FilterXObject *
filterx_typecast_datetime(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args,
                                                   "FilterX: Failed to create datetime object: invalid number of arguments. "
                                                   "Usage: datetime($isodate_str), datetime($unix_int_ms) or datetime($unix_float_s)");
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
      return filterx_typecast_datetime_isodate(args);
    }
  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "datetime"));
  return NULL;
}

FilterXObject *
filterx_typecast_datetime_isodate(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args,
                                                   "FilterX: Failed to create datetime object: invalid number of arguments. "
                                                   "Usage: datetime($isodate_str), datetime($unix_int_ms) or datetime($unix_float_s)");
  if (!object)
    return NULL;

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
                evt_tag_str("format", "isodate"));
      return NULL;
    }

  gchar *end = wall_clock_time_strptime(&wct, datefmt_isodate, timestr);
  if (end && *end != 0)
    {
      msg_error("filterx: unable to parse time",
                evt_tag_str("from", object->type->name),
                evt_tag_str("to", "datetime"),
                evt_tag_str("format", "isodate"),
                evt_tag_str("time_string", timestr),
                evt_tag_str("end", end));
      return NULL;
    }

  convert_wall_clock_time_to_unix_time(&wct, &ut);
  return filterx_datetime_new(&ut);
}

gboolean
datetime_repr(const UnixTime *ut, GString *repr)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  convert_unix_time_to_wall_clock_time(ut, &wct);
  append_format_wall_clock_time(&wct, repr, TS_FMT_ISO, 3);
  return TRUE;
}

static gboolean
_repr(FilterXObject *s, GString *repr)
{
  UnixTime ut = filterx_datetime_get_value(s);
  return datetime_repr(&ut, repr);
}

const gchar *
_strptime_get_time_str_from_object(FilterXObject *obj, gsize *len)
{
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)))
    return filterx_string_get_value(obj, len);

  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(message_value)))
    {
      if (filterx_message_value_get_type(obj) == LM_VT_STRING)
        return filterx_message_value_get_value(obj, len);
    }

  return NULL;
}


typedef struct FilterXFunctionStrptime_
{
  FilterXFunction super;
  FilterXExpr *time_str_expr;
  GPtrArray *formats;
} FilterXFunctionStrptime;

static FilterXObject *
_strptime_eval(FilterXExpr *s)
{
  FilterXFunctionStrptime *self = (FilterXFunctionStrptime *) s;

  FilterXObject *result = NULL;

  gsize time_str_len;
  FilterXObject *time_str_obj = filterx_expr_eval(self->time_str_expr);
  if (!time_str_obj)
    {
      filterx_eval_push_error("Failed to evaluate first argument. " FILTERX_FUNC_STRPTIME_USAGE, s, NULL);
      return NULL;
    }

  const gchar *time_str = _strptime_get_time_str_from_object(time_str_obj, &time_str_len);
  filterx_object_unref(time_str_obj);

  if (!time_str)
    {
      filterx_eval_push_error("First argument must be string typed. " FILTERX_FUNC_STRPTIME_USAGE, s, NULL);
      return NULL;
    }

  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  UnixTime ut = UNIX_TIME_INIT;
  gchar *end = NULL;

  for (guint i = 0; i < self->formats->len; i++)
    {
      const gchar *format = g_ptr_array_index(self->formats, i);
      end = wall_clock_time_strptime(&wct, format, time_str);
      if (!end)
        {
          msg_debug("filterx: unable to parse time",
                    evt_tag_str("time_string", time_str),
                    evt_tag_str("format", format));
        }
      else
        break;
    }

  if (end)
    {
      convert_wall_clock_time_to_unix_time(&wct, &ut);
      result = filterx_datetime_new(&ut);
    }
  else
    result = filterx_null_new();

  return result;
}

static void
_strptime_free(FilterXExpr *s)
{
  FilterXFunctionStrptime *self = (FilterXFunctionStrptime *) s;

  filterx_expr_unref(self->time_str_expr);
  g_ptr_array_free(self->formats, TRUE);
  filterx_function_free_method(&self->super);
}

static GPtrArray *
_extract_strptime_formats(FilterXFunctionArgs *args, GError **error)
{
  guint64 num_formats = filterx_function_args_len(args) - 1;
  GPtrArray *formats = g_ptr_array_new_full(num_formats, g_free);

  for (guint64 i = 0; i < num_formats; i++)
    {
      const gchar *format = filterx_function_args_get_literal_string(args, i+1, NULL);
      if (!format)
        {
          g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                      "format_str_%" G_GUINT64_FORMAT " argument must be string literal. " FILTERX_FUNC_STRPTIME_USAGE,
                      i+1);
          goto error;
        }

      g_ptr_array_add(formats, g_strdup(format));
    }

  return formats;

error:
  g_ptr_array_free(formats, TRUE);
  return NULL;
}

static FilterXExpr *
_extract_time_str_expr(FilterXFunctionArgs *args, GError **error)
{
  FilterXExpr *time_str_expr = filterx_function_args_get_expr(args, 0);
  if (!time_str_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be set: time_str. " FILTERX_FUNC_STRPTIME_USAGE);
      return NULL;
    }

  return time_str_expr;
}

static gboolean
_extract_args(FilterXFunctionStrptime *self, FilterXFunctionArgs *args, GError **error)
{
  gsize len = filterx_function_args_len(args);
  if (len < 2)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_STRPTIME_USAGE);
      return FALSE;
    }

  self->time_str_expr = _extract_time_str_expr(args, error);
  if (!self->time_str_expr)
    return FALSE;

  self->formats = _extract_strptime_formats(args, error);
  if (!self->formats)
    return FALSE;

  return TRUE;
}

/* Takes reference of args */
FilterXFunction *
filterx_function_strptime_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionStrptime *self = g_new0(FilterXFunctionStrptime, 1);
  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _strptime_eval;
  self->super.super.free_fn = _strptime_free;

  if (!_extract_args(self, args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

FILTERX_DEFINE_TYPE(datetime, FILTERX_TYPE_NAME(object),
                    .truthy = _truthy,
                    .map_to_json = _map_to_json,
                    .marshal = _marshal,
                    .repr = _repr,
                   );
