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
#include "python-logtemplate-options.h"
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
py_log_template_format(PyObject *s, PyObject *args, PyObject *kwrds)
{
  PyLogTemplate *self = (PyLogTemplate *)s;

  PyLogMessage *msg;
  PyLogTemplateOptions *py_log_template_options = NULL;
  gint tz = LTZ_SEND;
  gint seqnum = 0;

  static const gchar *kwlist[] = {"msg", "options", "tz", "seqnum", NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwrds, "O|Oii", (gchar **)kwlist,
                                   &msg, &py_log_template_options, &tz, &seqnum))
    return NULL;

  if (Py_TYPE(msg) != &py_log_message_type)
    {
      PyErr_Format(PyExc_TypeError,
                   "LogMessage expected in the first parameter");
      return NULL;
    }

  if (py_log_template_options && (Py_TYPE(py_log_template_options) != &py_log_template_options_type))
    {
      PyErr_Format(PyExc_TypeError,
                   "LogTemplateOptions expected in the second parameter");
      return NULL;
    }

  LogTemplateOptions *log_template_options = py_log_template_options ? py_log_template_options->template_options :
                                             self->template_options;
  if (!log_template_options)
    {
      PyErr_Format(PyExc_RuntimeError,
                   "LogTemplateOptions must be provided either in the LogTemplate constructor or as parameter of format");
      return NULL;
    }

  GString *result = scratch_buffers_alloc();
  log_template_format(self->template, msg->msg, log_template_options, tz, seqnum, NULL, result);

  return _py_string_from_string(result->str, result->len);
}

PyObject *
py_log_template_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const gchar *template_string;
  PyLogTemplateOptions *py_log_template_options = NULL;
  if (!PyArg_ParseTuple(args, "s|O", &template_string, &py_log_template_options))
    return NULL;

  if (py_log_template_options && (Py_TYPE(py_log_template_options) != &py_log_template_options_type))
    {
      PyErr_Format(PyExc_TypeError,
                   "LogTemplateOptions expected in the second parameter");
      return NULL;
    }

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
  if (py_log_template_options)
    self->template_options = py_log_template_options->template_options;

  return (PyObject *)self;
}

static PyMethodDef py_log_template_methods[] =
{
  { "format", (PyCFunction)py_log_template_format, METH_VARARGS | METH_KEYWORDS, "format template" },
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
  PyObject *PY_LTZ_LOCAL = int_as_pyobject(0);
  PyObject *PY_LTZ_SEND = int_as_pyobject(1);

  PyObject_SetAttrString(PyImport_AddModule("syslogng"), "LTZ_LOCAL", PY_LTZ_LOCAL);
  PyObject_SetAttrString(PyImport_AddModule("syslogng"), "LTZ_SEND", PY_LTZ_SEND);

  Py_DECREF(PY_LTZ_LOCAL);
  Py_DECREF(PY_LTZ_SEND);
}
