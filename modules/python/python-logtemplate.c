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
#include "python-logtemplate.h"
#include "python-logmsg.h"
#include "python-helpers.h"
#include "scratch-buffers.h"
#include "messages.h"

PyTypeObject py_log_template_type;

void
py_log_template_free(PyLogTemplate *self)
{
  log_template_unref(self->template);
  g_free(self->template_options);
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *
py_log_template_format(PyObject *s, PyObject *args)
{
  PyLogTemplate *self = (PyLogTemplate *)s;

  PyLogMessage *msg;
  if (!PyArg_ParseTuple(args, "O", &msg))
    return NULL;

  GString *result = scratch_buffers_alloc();
  log_template_format(self->template, msg->msg, self->template_options, LTZ_SEND, 0, NULL, result);

  return _py_string_from_string(result->str, result->len);
}

PyObject *
py_log_template_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const gchar *template_string;
  if (!PyArg_ParseTuple(args, "s", &template_string))
    return NULL;

  LogTemplate *template = log_template_new(NULL, NULL);
  GError *error = NULL;
  if (!log_template_compile(template, template_string, &error))
    {
      PyErr_Format(PyExc_RuntimeError,
                   "Error compiling template: %s", error->message);
      g_clear_error(&error);
      log_template_unref(template);
      return NULL;
    }

  PyLogTemplate *self = (PyLogTemplate *)type->tp_alloc(type, 0);
  if (!self)
    {
      log_template_unref(template);
      return NULL;
    }

  self->template = template;
  self->template_options = g_new0(LogTemplateOptions, 1);
  log_template_options_defaults(self->template_options);

  return (PyObject *)self;
}

static PyMethodDef py_log_template_methods[] =
{
  { "format", (PyCFunction)py_log_template_format, METH_VARARGS, "format template" },
  { NULL }
};

PyTypeObject py_log_template_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogTemplate",
  .tp_basicsize = sizeof(PyLogTemplate),
  .tp_dealloc = (destructor) py_log_template_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "LogTemplate class encapsulating a syslog-ng template",
  .tp_methods = py_log_template_methods,
  .tp_new = py_log_template_new,
  0,
};

void
py_log_template_init(void)
{
  py_log_template_options_init();

  PyType_Ready(&py_log_template_type);
  PyModule_AddObject(PyImport_AddModule("syslogng"), "LogTemplate", (PyObject *) &py_log_template_type);
}
