/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 László Várady
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

#include "python-ack-tracker.h"
#include "python-bookmark.h"
#include "compat/compat-python.h"
#include "python-helpers.h"

#include "ack-tracker/bookmark.h"


static void
py_ack_tracker_factory_free(PyAckTrackerFactory *self)
{
  ack_tracker_factory_unref(self->ack_tracker_factory);
  self->ack_tracker_factory = NULL;

  Py_CLEAR(self->ack_callback);

  Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
py_instant_ack_tracker_factory_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  PyObject *ack_callback;
  static const gchar *kwlist[] = {"ack_callback", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (gchar **) kwlist, &ack_callback))
    return NULL;

  if (!PyCallable_Check(ack_callback))
    {
      PyErr_Format(PyExc_TypeError, "A callable object is expected (ack_callback)");
      return NULL;
    }

  PyAckTrackerFactory *self = (PyAckTrackerFactory *) subtype->tp_alloc(subtype, 0);
  if (!self)
    return NULL;

  Py_XINCREF(ack_callback);
  self->ack_callback = ack_callback;

  self->ack_tracker_factory = instant_ack_tracker_factory_new();

  return (PyObject *) self;
}

static PyObject *
py_consecutive_ack_tracker_factory_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  PyObject *ack_callback;
  static const gchar *kwlist[] = {"ack_callback", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (gchar **) kwlist, &ack_callback))
    return NULL;

  if (!PyCallable_Check(ack_callback))
    {
      PyErr_Format(PyExc_TypeError, "A callable object is expected (ack_callback)");
      return NULL;
    }

  PyAckTrackerFactory *self = (PyAckTrackerFactory *) subtype->tp_alloc(subtype, 0);
  if (!self)
    return NULL;

  Py_XINCREF(ack_callback);
  self->ack_callback = ack_callback;

  self->ack_tracker_factory = consecutive_ack_tracker_factory_new();

  return (PyObject *) self;
}

static void
_invoke_batched_ack_callback(GList *ack_records, gpointer user_data)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyAckTrackerFactory *py_batched_ack_tracker_factory = (PyAckTrackerFactory *) user_data;
  PyObject *py_ack_records = PyList_New(0);

  for (GList *l = ack_records; l != NULL; l = l->next)
    {
      AckRecord *ack_record = l->data;
      Bookmark *bookmark = &ack_record->bookmark;

      PyBookmark *py_bookmark = bookmark_to_py_bookmark(bookmark);

      if (py_bookmark)
        PyList_Append(py_ack_records, py_bookmark->data);

      Py_XDECREF(py_bookmark);
    }

  _py_invoke_void_function(py_batched_ack_tracker_factory->ack_callback, py_ack_records,
                           "BatchedAckTracker", NULL);

  Py_XDECREF(py_ack_records);
  PyGILState_Release(gstate);
}

static PyObject *
py_batched_ack_tracker_factory_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  guint timeout, batch_size;
  PyObject *batched_ack_callback;

  static const gchar *kwlist[] = {"timeout", "batch_size", "batched_ack_callback", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "IIO", (gchar **) kwlist, &timeout, &batch_size, &batched_ack_callback))
    return NULL;

  if (!PyCallable_Check(batched_ack_callback))
    {
      PyErr_Format(PyExc_TypeError, "A callable object is expected (batched_ack_callback)");
      return NULL;
    }

  PyAckTrackerFactory *self = (PyAckTrackerFactory *) subtype->tp_alloc(subtype, 0);
  if (!self)
    return NULL;

  Py_XINCREF(batched_ack_callback);
  self->ack_callback = batched_ack_callback;

  self->ack_tracker_factory = batched_ack_tracker_factory_new(timeout, batch_size, _invoke_batched_ack_callback, self);

  return (PyObject *) self;
}

int
py_is_ack_tracker_factory(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_ack_tracker_factory_type);
}

PyTypeObject py_ack_tracker_factory_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "AckTrackerFactory",
  .tp_basicsize = sizeof(PyAckTrackerFactory),
  .tp_dealloc = (destructor) py_ack_tracker_factory_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "AckTrackerFactory",
  .tp_new = PyType_GenericNew,
  0,
};

PyTypeObject py_instant_ack_tracker_factory_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "InstantAckTrackerFactory",
  .tp_basicsize = sizeof(PyAckTrackerFactory),
  .tp_dealloc = (destructor) py_ack_tracker_factory_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "InstantAckTrackerFactory",
  .tp_new = py_instant_ack_tracker_factory_new,
  0,
};

PyTypeObject py_consecutive_ack_tracker_factory_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "ConsecutiveAckTrackerFactory",
  .tp_basicsize = sizeof(PyAckTrackerFactory),
  .tp_dealloc = (destructor) py_ack_tracker_factory_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "ConsecutiveAckTrackerFactory",
  .tp_new = py_consecutive_ack_tracker_factory_new,
  0,
};

PyTypeObject py_batched_ack_tracker_factory_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "BatchedAckTrackerFactory",
  .tp_basicsize = sizeof(PyAckTrackerFactory),
  .tp_dealloc = (destructor) py_ack_tracker_factory_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "BatchedAckTrackerFactory",
  .tp_new = py_batched_ack_tracker_factory_new,
  0,
};

void
py_ack_tracker_init(void)
{
  PyType_Ready(&py_ack_tracker_factory_type);

  py_instant_ack_tracker_factory_type.tp_base = &py_ack_tracker_factory_type;
  PyType_Ready(&py_instant_ack_tracker_factory_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "InstantAckTracker",
                     (PyObject *) &py_instant_ack_tracker_factory_type);

  py_consecutive_ack_tracker_factory_type.tp_base = &py_ack_tracker_factory_type;
  PyType_Ready(&py_consecutive_ack_tracker_factory_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "ConsecutiveAckTracker",
                     (PyObject *) &py_consecutive_ack_tracker_factory_type);

  py_batched_ack_tracker_factory_type.tp_base = &py_ack_tracker_factory_type;
  PyType_Ready(&py_batched_ack_tracker_factory_type);
  PyModule_AddObject(PyImport_AddModule("_syslogng"), "BatchedAckTracker", (PyObject *)
                     &py_batched_ack_tracker_factory_type);
}
