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
#include "python-helpers.h"
#include "messages.h"

PyTypeObject py_log_template_type;

static void
py_log_template_free(PyLogTemplate *self)
{
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyTypeObject py_log_template_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "LogTemplate",
  .tp_basicsize = sizeof(PyLogTemplate),
  .tp_dealloc = (destructor) py_log_template_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "LogTemplate class encapsulating a syslog-ng template",
  .tp_new = PyType_GenericNew,
  0,
};

void
py_log_template_init(void)
{
  PyType_Ready(&py_log_template_type);
  PyModule_AddObject(PyImport_AddModule("syslogng"), "LogTemplate", (PyObject *) &py_log_template_type);
}
