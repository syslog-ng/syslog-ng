/*
 * Copyright (c) 2017 Balabit
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

#include "compat-python.h"
#include "python-helpers.h"

#include <datetime.h>

void
py_init_argv(void)
{
  static wchar_t *argv[] = {L"syslog-ng"};
  PySys_SetArgvEx(1, argv, 0);
}

PyObject *
int_as_pyobject(gint num)
{
  return PyLong_FromLong(num);
};

gboolean
py_datetime_to_logstamp(PyObject *py_timestamp, UnixTime *logstamp)
{
  if (!PyDateTime_Check(py_timestamp))
    {
      PyErr_Format(PyExc_TypeError, "datetime expected in the first parameter");
      return FALSE;
    }

  PyObject *py_posix_timestamp = _py_invoke_method_by_name(py_timestamp, "timestamp", NULL,
                                                           "PyDateTime", "py_datetime_to_logstamp");
  if (!py_posix_timestamp)
    {
      PyErr_Format(PyExc_ValueError, "Error calculating POSIX timestamp");
      return FALSE;
    }

  PyObject *py_utcoffset = _py_invoke_method_by_name(py_timestamp, "utcoffset", NULL,
                                                     "PyDateTime", "py_datetime_to_logstamp");
  if (!py_utcoffset)
    {
      Py_DECREF(py_posix_timestamp);
      PyErr_Format(PyExc_ValueError, "Error retrieving utcoffset");
      return FALSE;
    }

  gdouble posix_timestamp = PyFloat_AsDouble(py_posix_timestamp);
  gint zone_offset = py_utcoffset == Py_None ? 0 : PyDateTime_DELTA_GET_SECONDS(py_utcoffset);

  Py_DECREF(py_utcoffset);
  Py_DECREF(py_posix_timestamp);

  logstamp->ut_sec = (time_t) posix_timestamp;
  logstamp->ut_usec = posix_timestamp * 10e5 - logstamp->ut_sec * 10e5;
  logstamp->ut_gmtoff = zone_offset;

  return TRUE;
}

gint
pyobject_as_int(PyObject *object)
{
  return PyLong_AsLong(object);
};
