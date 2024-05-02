/*
 * Copyright (c) 2022 One Identity
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
#include "python-types.h"
#include "python-helpers.h"
#include "scanner/list-scanner/list-scanner.h"
#include "str-repr/encode.h"
#include "timeutils/conv.h"
#include "timeutils/cache.h"
#include "messages.h"

/* Python datetime API */
#include <datetime.h>

PyObject *
py_bytes_from_string(const char *value, gssize len)
{
  if (len < 0)
    len = strlen(value);

  return PyBytes_FromStringAndSize(value, len);
}

PyObject *
py_string_from_string(const char *value, gssize len)
{
  PyObject *obj = NULL;
  const gchar *charset;

  if (len < 0)
    len = strlen(value);

  /* NOTE: g_get_charset() returns if the current character set is utf8 */
  if (g_get_charset(&charset))
    {
      obj = PyUnicode_FromStringAndSize(value, len);
    }
  else
    {
      GError *error = NULL;
      gsize bytes_read, bytes_written;
      gchar *utf8_string;

      utf8_string = g_locale_to_utf8(value, len, &bytes_read, &bytes_written, &error);
      if (utf8_string)
        {
          obj = PyUnicode_FromStringAndSize(utf8_string, bytes_written);
          g_free(utf8_string);
        }
      else
        {
          g_error_free(error);
          obj = PyBytes_FromStringAndSize(value, len);
        }
    }

  return obj;
}

PyObject *
py_long_from_long(gint64 l)
{
  return PyLong_FromLong(l);
}

PyObject *
py_double_from_double(gdouble d)
{
  return PyFloat_FromDouble(d);
}

PyObject *
py_boolean_from_boolean(gboolean b)
{
  return PyBool_FromLong(b);
}

PyObject *
py_list_from_list(const gchar *list, gssize list_len)
{
  PyObject *obj = PyList_New(0);
  if (!obj)
    {
      return NULL;
    }

  ListScanner scanner;
  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, list, list_len);

  while (list_scanner_scan_next(&scanner))
    {
      PyObject *element = py_bytes_from_string(list_scanner_get_current_value(&scanner),
                                               list_scanner_get_current_value_len(&scanner));
      if (!element || PyList_Append(obj, element) != 0)
        {
          list_scanner_deinit(&scanner);
          Py_DECREF(obj);
          Py_XDECREF(element);
          return NULL;
        }

      Py_DECREF(element);
    }

  list_scanner_deinit(&scanner);

  return obj;
}

PyObject *
py_string_list_from_string_list(const GList *string_list)
{
  PyObject *obj = PyList_New(0);
  if (!obj)
    {
      return NULL;
    }

  for (const GList *elem = string_list; elem; elem = elem->next)
    {
      const gchar *string = (const gchar *) elem->data;
      PyObject *py_string = py_string_from_string(string, -1);
      if (!py_string || PyList_Append(obj, py_string) != 0)
        {
          Py_DECREF(obj);
          Py_XDECREF(py_string);
          return NULL;
        }
    }

  return obj;
}

PyObject *
py_datetime_from_unix_time(UnixTime *ut)
{
  WallClockTime wct;

  convert_unix_time_to_wall_clock_time(ut, &wct);

  return PyDateTime_FromDateAndTimeAndFold(
           wct.wct_year + 1900, wct.wct_mon + 1, wct.wct_mday,
           wct.wct_hour, wct.wct_min, wct.wct_sec,
           wct.wct_usec, wct.wct_isdst > 0);
}

PyObject *
py_datetime_from_msec(gint64 msec)
{
  UnixTime ut;

  ut.ut_sec = msec / 1000;
  ut.ut_usec = (msec % 1000) * 1000;
  ut.ut_gmtoff = get_local_timezone_ofs(ut.ut_sec);
  return py_datetime_from_unix_time(&ut);
}

/* in case we return NULL a Python exception needs to be raised */
PyObject *
py_obj_from_log_msg_value(const gchar *value, gssize value_len, LogMessageValueType type)
{
  switch (type)
    {
    case LM_VT_INTEGER:
    {
      gint64 l;
      if (type_cast_to_int64(value, value_len, &l, NULL))
        return py_long_from_long(l);
      goto type_cast_error;
    }

    case LM_VT_DOUBLE:
    {
      gdouble d;
      if (type_cast_to_double(value, value_len, &d, NULL))
        return py_double_from_double(d);
      goto type_cast_error;
    }

    case LM_VT_BOOLEAN:
    {
      gboolean b;
      if (type_cast_to_boolean(value, value_len, &b, NULL))
        return py_boolean_from_boolean(b);
      goto type_cast_error;
    }

    case LM_VT_NULL:
      Py_RETURN_NONE;

    case LM_VT_LIST:
      return py_list_from_list(value, value_len);

    case LM_VT_DATETIME:
    {
      gint64 msec = 0;

      if (type_cast_to_datetime_msec(value, value_len, &msec, NULL))
        return py_datetime_from_msec(msec);
      goto type_cast_error;
    }

    case LM_VT_BYTES:
    case LM_VT_PROTOBUF:
      /*
      * Dedicated python class is needed to differentiate LM_VT_STRING <-> bytes and LM_VT_BYTES <-> bytes.
      * Until then we should not be converting, and we should mimic that the key does not exist.
      * We should not end up here by default, only if the user explicitly set their value-pairs() in the config
      * with include-bytes().
      */
      return NULL;

    case LM_VT_STRING:
    default:
      return py_bytes_from_string(value, value_len);
    }

  g_assert_not_reached();
type_cast_error:

  /* if we can't cast the value (e.g.  in contrast with the type-hint the
   * value was invalid), fall back to using a string */
  return py_bytes_from_string(value, value_len);
}

gboolean
is_py_obj_bytes_or_string_type(PyObject *obj)
{
  return PyBytes_CheckExact(obj) || PyUnicode_CheckExact(obj);
}

/* NOTE: This function returns a managed memory area pointing to an utf8
 *       representation of the string, with the following constraints:
 *
 *   1) We basically assume that non-unicode strings are in utf8 or at least
 *      utf8 compatible (e.g. ascii).  It doesn't really make sense otherwise.
 *      If we don't the resulting string is not going to be utf8, rather it
 *      would be the system codepage.
 *
 *   2) We are using the utf8 cache in the unicode instance.
 */
gboolean
py_bytes_or_string_to_string(PyObject *obj, const gchar **string)
{
  if (!is_py_obj_bytes_or_string_type(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from str/bytes");
      return FALSE;
    }

  const gchar *string_local;

  if (PyBytes_Check(obj))
    {
      string_local = PyBytes_AsString(obj);
    }
  else if (PyUnicode_Check(obj))
    {
      string_local = PyUnicode_AsUTF8(obj);
    }
  else
    {
      msg_error("Unexpected python string value");
      return FALSE;
    }

  if (!string_local)
    {
      return FALSE;
    }

  *string = string_local;
  return TRUE;
}

gboolean
py_long_to_long(PyObject *obj, gint64 *l)
{
  if (!PyLong_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from long");
      return FALSE;
    }

  gint64 l_local = PyLong_AsLong(obj);
  if (PyErr_Occurred())
    {
      return FALSE;
    }

  *l = l_local;
  return TRUE;
}

gboolean
py_double_to_double(PyObject *obj, gdouble *d)
{
  if (!PyFloat_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from float");
      return FALSE;
    }

  gdouble d_local = PyFloat_AsDouble(obj);
  if (PyErr_Occurred())
    {
      return FALSE;
    }

  *d = d_local;
  return TRUE;
}

gboolean
py_boolean_to_boolean(PyObject *obj, gboolean *b)
{
  if (!PyBool_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from bool");
      return FALSE;
    }

  if (obj == Py_True)
    {
      *b = TRUE;
      return TRUE;
    }

  if (obj == Py_False)
    {
      *b = FALSE;
      return TRUE;
    }

  return FALSE;
}

gboolean
py_list_to_list(PyObject *obj, GString *list)
{
  g_string_truncate(list, 0);

  if (!PyList_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from list");
      return FALSE;
    }

  for (Py_ssize_t i = 0; i < PyList_GET_SIZE(obj); i++)
    {
      PyObject *element = PyList_GET_ITEM(obj, i);

      const gchar *element_as_str;
      if (!py_bytes_or_string_to_string(element, &element_as_str))
        return FALSE;

      if (i != 0)
        g_string_append_c(list, ',');

      str_repr_encode_append(list, element_as_str, -1, ",");
    }

  return TRUE;
}

gboolean
py_string_list_to_string_list(PyObject *obj, GList **string_list)
{
  *string_list = NULL;

  if (!PyList_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from list");
      return FALSE;
    }

  for (Py_ssize_t i = 0; i < PyList_GET_SIZE(obj); i++)
    {
      PyObject *element = PyList_GET_ITEM(obj, i);

      const gchar *element_as_str;
      if (!py_bytes_or_string_to_string(element, &element_as_str))
        {
          g_list_free_full(*string_list, g_free);
          *string_list = NULL;
          return FALSE;
        }

      *string_list = g_list_append(*string_list, g_strdup(element_as_str));
    }

  return TRUE;
}

static gboolean
_datetime_get_gmtoff(PyObject *py_datetime, glong *utcoffset)
{
  *utcoffset = -1;
  PyObject *py_utcoffset = _py_invoke_method_by_name(py_datetime, "utcoffset", NULL, "PyDateTime",
                                                     "py_datetime_to_datetime");
  if (!py_utcoffset)
    return FALSE;

  if (py_utcoffset != Py_None)
    *utcoffset = PyDateTime_DELTA_GET_SECONDS(py_utcoffset);

  Py_XDECREF(py_utcoffset);
  return TRUE;
}

gboolean
py_datetime_to_unix_time(PyObject *obj, UnixTime *ut)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  if (!PyDateTime_Check(obj))
    {
      PyErr_Format(PyExc_ValueError, "Error extracting value from datetime");
      return FALSE;
    }

  if (!_datetime_get_gmtoff(obj, &wct.wct_gmtoff))
    {
      return FALSE;
    }

  wct.wct_year = PyDateTime_GET_YEAR(obj) - 1900;
  wct.wct_mon = PyDateTime_GET_MONTH(obj) - 1;
  wct.wct_mday = PyDateTime_GET_DAY(obj);
  wct.wct_hour = PyDateTime_DATE_GET_HOUR(obj);
  wct.wct_min = PyDateTime_DATE_GET_MINUTE(obj);
  wct.wct_sec = PyDateTime_DATE_GET_SECOND(obj);
  wct.wct_usec = PyDateTime_DATE_GET_MICROSECOND(obj);
  wct.wct_isdst = PyDateTime_DATE_GET_FOLD(obj);
  convert_wall_clock_time_to_unix_time(&wct, ut);

  if (ut->ut_gmtoff == -1)
    ut->ut_gmtoff = get_local_timezone_ofs(ut->ut_sec);
  return TRUE;
}

gboolean
py_datetime_to_datetime(PyObject *obj, GString *dt)
{
  UnixTime ut;

  if (!py_datetime_to_unix_time(obj, &ut))
    return FALSE;
  g_string_printf(dt, "%"G_GINT64_FORMAT".%03"G_GUINT32_FORMAT, ut.ut_sec, ut.ut_usec / 1000);
  return TRUE;
}


gboolean
py_obj_to_log_msg_value(PyObject *obj, GString *value, LogMessageValueType *type)
{
  if (is_py_obj_bytes_or_string_type(obj))
    {
      const gchar *string;
      if (!py_bytes_or_string_to_string(obj, &string))
        return FALSE;

      *type = LM_VT_STRING;
      g_string_assign(value, string);

      return TRUE;
    }

  if (PyLong_CheckExact(obj))
    {
      gint64 l;
      if (!py_long_to_long(obj, &l))
        return FALSE;

      *type = LM_VT_INTEGER;
      g_string_printf(value, "%"G_GINT64_FORMAT, l);

      return TRUE;
    }

  if (PyFloat_CheckExact(obj))
    {
      gdouble d;
      if (!py_double_to_double(obj, &d))
        return FALSE;

      *type = LM_VT_DOUBLE;
      g_string_printf(value, "%f", d);

      return TRUE;
    }

  if (PyBool_Check(obj))
    {
      gboolean b;
      if (!py_boolean_to_boolean(obj, &b))
        return FALSE;

      *type = LM_VT_BOOLEAN;
      g_string_assign(value, b ? "true" : "false");

      return TRUE;
    }

  if (obj == Py_None)
    {
      *type = LM_VT_NULL;
      g_string_truncate(value, 0);
      return TRUE;
    }

  if (PyList_CheckExact(obj))
    {
      if (!py_list_to_list(obj, value))
        return FALSE;

      *type = LM_VT_LIST;

      return TRUE;
    }

  if (PyDateTime_Check(obj))
    {
      if (!py_datetime_to_datetime(obj, value))
        return FALSE;
      *type = LM_VT_DATETIME;
      return TRUE;
    }

  *type = LM_VT_NONE;

  msg_error("Unexpected python object type", evt_tag_str("type", Py_TYPE(obj)->tp_name));
  PyErr_Format(PyExc_ValueError, "Error extracting value from Python object");
  return FALSE;
}

void
py_init_types(void)
{
  PyDateTime_IMPORT;
}
