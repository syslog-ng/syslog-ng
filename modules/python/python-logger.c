/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan
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

#include "python-logger.h"

#include "python-helpers.h"
#include "messages.h"

PyTypeObject py_logger_type;


PyObject *
py_msg_error(PyObject *obj, PyObject *args)
{
  char *message = NULL;
  if (!PyArg_ParseTuple(args, "s", &message))
    return NULL;

  msg_error(message);
  return Py_None;
}

PyObject *
py_msg_warning(PyObject *obj, PyObject *args)
{
  char *message = NULL;
  if (!PyArg_ParseTuple(args, "s", &message))
    return NULL;

  msg_warning(message);
  return Py_None;
}

PyObject *
py_msg_info(PyObject *obj, PyObject *args)
{
  char *message = NULL;
  if (!PyArg_ParseTuple(args, "s", &message))
    return NULL;

  msg_info(message);
  return Py_None;
}

PyObject *
py_msg_debug(PyObject *obj, PyObject *args)
{
  if (!debug_flag)
    return Py_None;

  char *message = NULL;
  if (!PyArg_ParseTuple(args, "s", &message))
    return NULL;

  msg_debug(message);
  return Py_None;
}

PyObject *
py_msg_trace(PyObject *obj, PyObject *args)
{
  if (!trace_flag)
    return Py_None;

  char *message = NULL;
  if (!PyArg_ParseTuple(args, "s", &message))
    return NULL;

  msg_trace(message);
  return Py_None;
}



static PyMethodDef py_logger_methods[] =
{
  { "error",   (PyCFunction)py_msg_error,   METH_VARARGS, "msg_error" },
  { "warning", (PyCFunction)py_msg_warning, METH_VARARGS, "msg_warning" },
  { "info",    (PyCFunction)py_msg_info,    METH_VARARGS, "msg_info" },
  { "debug",   (PyCFunction)py_msg_debug,   METH_VARARGS, "msg_debug" },
  { "trace",   (PyCFunction)py_msg_trace,   METH_VARARGS, "msg_trace" },

  { NULL }
};

PyTypeObject py_logger_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_basicsize = sizeof(PyObject),
  .tp_dealloc = py_slng_generic_dealloc,
  .tp_doc = "Those messages can be captured by internal() source.",
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_methods = py_logger_methods,
  .tp_name = "Logger",
  .tp_new = PyType_GenericNew,
  0
};

void
py_logger_init(void)
{
  PyType_Ready(&py_logger_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "Logger", (PyObject *) &py_logger_type);
}
