/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-module.h"
#include "python-helpers.h"

static PyTypeObject py_global_code_loader_type;

/* This class implements the Loader interface attached to the main module
 * (e.g.  _syslogng_main), so the details of exceptions can be properly reported.
 */

typedef struct _PyGlobalCodeLoader
{
  PyObject_HEAD
  gchar *source;
} PyGlobalCodeLoader;


static PyObject *
_get_source(PyGlobalCodeLoader *self, PyObject *args)
{
  char *name;

  if (!PyArg_ParseTuple(args, "s:get_source", &name))
    return NULL;

  return _py_string_from_string(self->source, -1);
}


PyObject *
py_global_code_loader_new(const gchar *source)
{
  PyGlobalCodeLoader *self;

  self = PyObject_New(PyGlobalCodeLoader, &py_global_code_loader_type);
  if (!self)
    return NULL;

  self->source = g_strdup(source);

  return (PyObject *) self;
}

static void
_free(PyGlobalCodeLoader *self)
{
  g_free(self->source);
  PyObject_Del(self);
}

static PyMethodDef _methods[] =
{
  {
    "get_source", (PyCFunction) _get_source,
    METH_VARARGS, "Return the source for the global code as present in the syslog-ng configuration file."
  },
  {NULL}
};

static PyTypeObject py_global_code_loader_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "_SyslogNGGlobalCodeLoader",
  .tp_basicsize = sizeof(PyGlobalCodeLoader),
  .tp_dealloc = (destructor) _free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "Class implementing the Loader interface to represent source code embedded into syslog-ng.conf",
  .tp_new = PyType_GenericNew,
  .tp_methods = _methods,
  0,
};

void
py_global_code_loader_init(void)
{
  PyType_Ready(&py_global_code_loader_type);
}
