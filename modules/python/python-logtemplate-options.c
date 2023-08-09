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

#include "python-logtemplate-options.h"
#include "python-main.h"

int
py_is_log_template_options(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_log_template_options_type);
}

PyObject *
py_log_template_options_new(LogTemplateOptions *template_options, GlobalConfig *cfg)
{
  PyLogTemplateOptions *self = PyObject_New(PyLogTemplateOptions, &py_log_template_options_type);

  if (!self)
    return NULL;

  memset(&self->template_options, 0, sizeof(self->template_options));
  log_template_options_clone(template_options, &self->template_options);
  log_template_options_init(&self->template_options, cfg);

  return (PyObject *) self;
}

int
py_log_template_options_init(PyObject *s, PyObject *args, PyObject *kwds)
{
  PyLogTemplateOptions *self = (PyLogTemplateOptions *) s;

  if (!PyArg_ParseTuple(args, ""))
    return -1;

  GlobalConfig *cfg = _py_get_config_from_main_module()->cfg;
  memset(&self->template_options, 0, sizeof(self->template_options));
  log_template_options_defaults(&self->template_options);
  log_template_options_init(&self->template_options, cfg);

  return 0;
}

PyTypeObject py_log_template_options_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogTemplateOptions",
  .tp_basicsize = sizeof(PyLogTemplateOptions),
  .tp_dealloc = (destructor) PyObject_Del,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "LogTemplateOptions class encapsulating a syslog-ng LogTemplateOptions",
  .tp_init = py_log_template_options_init,
  .tp_new = PyType_GenericNew,
  0,
};

void
py_log_template_options_global_init(void)
{
  PyType_Ready(&py_log_template_options_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "LogTemplateOptions", (PyObject *) &py_log_template_options_type);
}
