/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-helpers.h"
#include "messages.h"

const gchar *
_py_get_callable_name(PyObject *callable, gchar *buf, gsize buf_len)
{
  PyObject *name = PyObject_GetAttrString(callable, "__name__");

  if (name)
    {
      g_strlcpy(buf, PyBytes_AsString(name), buf_len);
    }
  else
    {
      PyErr_Clear();
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(name);
  return buf;
}

const gchar *
_py_format_exception_text(gchar *buf, gsize buf_len)
{
  PyObject *exc, *value, *tb, *str;

  PyErr_Fetch(&exc, &value, &tb);
  if (!exc)
    {
      g_strlcpy(buf, "None", buf_len);
      return buf;
    }
  PyErr_NormalizeException(&exc, &value, &tb);

  str = PyObject_Str(value);
  if (str)
    {
      g_snprintf(buf, buf_len, "%s: %s", ((PyTypeObject *) exc)->tp_name, PyBytes_AsString(str));
    }
  else
    {
      g_strlcpy(buf, "<unknown>", buf_len);
    }
  Py_XDECREF(exc);
  Py_XDECREF(value);
  Py_XDECREF(tb);
  Py_XDECREF(str);
  return buf;
}

PyObject *
_py_get_attr_or_null(PyObject *o, const gchar *attr)
{
  PyObject *result;

  if (!attr)
    return NULL;

  result = PyObject_GetAttrString(o, attr);
  if (!result)
    {
      PyErr_Clear();
      return NULL;
    }
  return result;
}

PyObject *
_py_do_import(const gchar *modname)
{
  PyObject *module, *modobj;

  module = PyUnicode_FromString(modname);
  if (!module)
    {
      msg_error("Error allocating Python string",
                evt_tag_str("string", modname));
      return NULL;
    }

  modobj = PyImport_Import(module);
  Py_DECREF(module);
  if (!modobj)
    {
      gchar buf[256];

      msg_error("Error loading Python module",
                evt_tag_str("module", modname),
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      return NULL;
    }
  return modobj;
}

gboolean
_split_fully_qualified_name(const gchar *input, gchar **module, gchar **class)
{
  const gchar *p;

  for (p = input + strlen(input) - 1; p > input && *p != '.'; p--)
    ;

  if (p > input)
    {
      *module = g_strndup(input, (p - input));
      *class = g_strdup(p + 1);
      return TRUE;
    }
  return FALSE;
}

PyObject *
_py_resolve_qualified_name(const gchar *name)
{
  PyObject *module, *value = NULL;
  gchar *module_name, *attribute_name;

  if (!_split_fully_qualified_name(name, &module_name, &attribute_name))
    {
      module_name = g_strdup("_syslogng");
      attribute_name = g_strdup(name);
    }

  module = _py_do_import(module_name);
  if (!module)
    goto exit;

  value = _py_get_attr_or_null(module, attribute_name);
  Py_DECREF(module);

 exit:
  g_free(module_name);
  g_free(attribute_name);
  return value;
}
