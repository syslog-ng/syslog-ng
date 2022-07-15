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
#include "messages.h"

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
          return NULL;
        }
    }

  list_scanner_deinit(&scanner);

  return obj;
}

PyObject *
py_obj_from_log_msg_value(const gchar *value, gssize value_len, LogMessageValueType type)
{
  switch (type)
    {
    case LM_VT_INTEGER:
    {
      gint64 l;
      if (type_cast_to_int64(value, &l, NULL))
        return py_long_from_long(l);
      return NULL;
    }

    case LM_VT_DOUBLE:
    {
      gdouble d;
      if (type_cast_to_double(value, &d, NULL))
        return py_double_from_double(d);
      return NULL;
    }

    case LM_VT_BOOLEAN:
    {
      gboolean b;
      if (type_cast_to_boolean(value, &b, NULL))
        return py_boolean_from_boolean(b);
      return NULL;
    }

    case LM_VT_NULL:
      Py_RETURN_NONE;

    case LM_VT_LIST:
      return py_list_from_list(value, value_len);

    case LM_VT_STRING:
    default:
      return py_bytes_from_string(value, value_len);
    }

  g_assert_not_reached();
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

/* Appends to the end of `list`. */
gboolean
py_list_to_list(PyObject *obj, GString *list)
{
  if (!PyList_Check(obj))
    {
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

/* Appends to the end of `value`. */
gboolean
py_obj_to_log_msg_value(PyObject *obj, GString *value, LogMessageValueType *type)
{
  if (is_py_obj_bytes_or_string_type(obj))
    {
      const gchar *string;
      if (!py_bytes_or_string_to_string(obj, &string))
        return FALSE;

      *type = LM_VT_STRING;
      g_string_append(value, string);

      return TRUE;
    }

  if (PyLong_CheckExact(obj))
    {
      gint64 l;
      if (!py_long_to_long(obj, &l))
        return FALSE;

      *type = LM_VT_INTEGER;
      g_string_append_printf(value, "%ld", l);

      return TRUE;
    }

  if (PyFloat_CheckExact(obj))
    {
      gdouble d;
      if (!py_double_to_double(obj, &d))
        return FALSE;

      *type = LM_VT_DOUBLE;
      g_string_append_printf(value, "%f", d);

      return TRUE;
    }

  if (PyBool_Check(obj))
    {
      gboolean b;
      if (!py_boolean_to_boolean(obj, &b))
        return FALSE;

      *type = LM_VT_BOOLEAN;
      g_string_append(value, b ? "true" : "false");

      return TRUE;
    }

  if (obj == Py_None)
    {
      *type = LM_VT_NULL;

      return TRUE;
    }

  if (PyList_CheckExact(obj))
    {
      if (!py_list_to_list(obj, value))
        return FALSE;

      *type = LM_VT_LIST;

      return TRUE;
    }

  *type = LM_VT_NONE;

  msg_error("Unexpected python object type", evt_tag_str("type", Py_TYPE(obj)->tp_name));
  return FALSE;
}
