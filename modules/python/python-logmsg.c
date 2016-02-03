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
#include "python-logmsg.h"
#include "logmsg/logmsg.h"

typedef struct _PyLogMessage
{
  PyObject_HEAD
  LogMessage *msg;
} PyLogMessage;

static PyTypeObject py_log_message_type;

static PyObject *
py_log_message_getattr(PyLogMessage *self, gchar *name)
{
  NVHandle handle;
  const gchar *value;

  handle = log_msg_get_value_handle(name);
  value = log_msg_get_value(self->msg, handle, NULL);
  if (!value)
    {
      PyErr_SetString(PyExc_AttributeError, "No such attribute");
      return NULL;
    }
  return PyBytes_FromString(value);
}

static void
py_log_message_free(PyLogMessage *self)
{
  log_msg_unref(self->msg);
  PyObject_Del(self);
}

PyObject *
py_log_message_new(LogMessage *msg)
{
  PyLogMessage *self;

  self = PyObject_New(PyLogMessage, &py_log_message_type);
  if (!self)
    return NULL;

  self->msg = log_msg_ref(msg);
  return (PyObject *) self;
}

static PyTypeObject py_log_message_type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  .tp_name = "LogMessage",
  .tp_basicsize = sizeof(PyLogMessage),
  .tp_dealloc = (destructor) py_log_message_free,
  .tp_getattr = (getattrfunc) py_log_message_getattr,
  .tp_setattr = (setattrfunc) NULL,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "LogMessage class encapsulating a syslog-ng log message",
  .tp_new = PyType_GenericNew,
};

void
python_log_message_init(void)
{
  PyType_Ready(&py_log_message_type);
}
