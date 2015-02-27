/*
 * Copyright (c) 2015 BalaBit
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
#include "logmsg.h"

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
  return PyString_FromString(value);
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

  .ob_size = 0,
  .tp_name = "LogMessage",
  .tp_basicsize = sizeof(PyLogMessage),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) py_log_message_free,
  .tp_print = NULL,
  .tp_getattr = (getattrfunc) py_log_message_getattr,
  .tp_setattr = (setattrfunc) NULL,
  .tp_compare = NULL,
  .tp_repr = NULL,
  .tp_as_number = NULL,
  .tp_as_sequence = NULL,
  .tp_as_mapping = NULL,
  .tp_hash = NULL,
  .tp_call = NULL,
  .tp_str = NULL,
  .tp_getattro = NULL,
  .tp_setattro = NULL,
  .tp_as_buffer = NULL,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "LogMessage class encapsulating a syslog-ng log message",
  .tp_traverse = NULL,
  .tp_clear = NULL,
  .tp_richcompare = NULL,
  .tp_weaklistoffset = 0,
  .tp_iter = NULL,
  .tp_iternext = NULL,
  .tp_methods = NULL,
  .tp_members = NULL,
  .tp_getset = NULL,
  .tp_base = NULL,
  .tp_dict = NULL,
  .tp_descr_get = NULL,
  .tp_descr_set = NULL,
  .tp_dictoffset = 0,
  .tp_init = NULL,
  .tp_alloc = NULL,
  .tp_new = PyType_GenericNew,
  .tp_free = NULL,
  .tp_is_gc = NULL,
  .tp_bases = NULL,
  .tp_mro = NULL,
  .tp_cache = NULL,
  .tp_subclasses = NULL,
  .tp_weaklist = NULL,
  .tp_del = NULL,
};

void
python_log_message_init(void)
{
  PyType_Ready(&py_log_message_type);
}
