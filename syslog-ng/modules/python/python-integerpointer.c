/*
 * Copyright (c) 2018 Balabit
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

#include "python-integerpointer.h"

PyObject *
py_integer_pointer_new(gpointer ptr)
{
  PyIntegerPointer *self = PyObject_New(PyIntegerPointer, &py_integer_pointer_type);
  if (!self)
    return NULL;

  self->ptr = ptr;

  return (PyObject *) self;
}

static PyObject *
_as_int(PyObject *s)
{
  PyIntegerPointer *self = (PyIntegerPointer *)s;
  return PyLong_FromLong(*self->ptr);
}

static PyObject *
_as_str(PyObject *s)
{
  PyIntegerPointer *self = (PyIntegerPointer *)s;
  return PyUnicode_FromFormat("%d", *self->ptr);
}

PyTypeObject py_integer_pointer_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "IntegerPointer",
  .tp_basicsize = sizeof(PyIntegerPointer),
  .tp_dealloc = (destructor) PyObject_Del,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "IntegerPointer class exposes an integer by a pointer",
  .tp_as_number = &(PyNumberMethods){ .nb_int = _as_int },
  .tp_str = _as_str,
  0,
};

void
py_integer_pointer_init(void)
{
  PyType_Ready(&py_integer_pointer_type);
}
