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
#include "syslog-ng.h"
#include "reloc.h"

void
_set_python_home(const gchar *python_home)
{
  Py_SetPythonHome(Py_DecodeLocale(python_home, NULL));
}

void
_force_python_home(const gchar *python_home)
{
  const gchar *resolved_python_home = get_installation_path_for(python_home);
  _set_python_home(resolved_python_home);
}

void
py_setup_python_home(void)
{
#ifdef SYSLOG_NG_PYTHON3_HOME_DIR
  if (strlen(SYSLOG_NG_PYTHON3_HOME_DIR) > 0)
    _force_python_home(SYSLOG_NG_PYTHON3_HOME_DIR);
#endif
}

void
py_init_argv(void)
{
  static wchar_t *argv[] = {L"syslog-ng"};
  PySys_SetArgvEx(1, argv, 0);
}

void
py_init_threads(void)
{
#if (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 7)
  PyEval_InitThreads();
#endif
}

PyObject *
int_as_pyobject(gint num)
{
  return PyLong_FromLong(num);
};

gint
pyobject_as_int(PyObject *object)
{
  return PyLong_AsLong(object);
};

gboolean
py_object_is_integer(PyObject *object)
{
  return PyLong_Check(object);
}
