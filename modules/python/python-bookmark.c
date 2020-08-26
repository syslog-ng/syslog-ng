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

#include "python-bookmark.h"
#include "compat/compat-python.h"
#include "python-helpers.h"

#include <structmember.h>


typedef struct _PyBookmarkData
{
  PyBookmark *py_bookmark;
} PyBookmarkData;

int
py_is_bookmark(PyObject *obj)
{
  return PyType_IsSubtype(Py_TYPE(obj), &py_bookmark_type);
}

static void
py_bookmark_free(PyBookmark *self)
{
  Py_CLEAR(self->data);
  Py_CLEAR(self->save);
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyBookmark *
py_bookmark_new(PyObject *data, PyObject *save)
{
  PyBookmark *self = PyObject_New(PyBookmark, &py_bookmark_type);
  if (!self)
    return NULL;

  Py_XINCREF(data);
  self->data = data;

  Py_XINCREF(save);
  self->save = save;

  return self;
}

PyBookmark *
bookmark_to_py_bookmark(Bookmark *bookmark)
{
  PyBookmarkData *data = (PyBookmarkData *) &bookmark->container;
  return data->py_bookmark;
}

static void
py_bookmark_save(Bookmark *bookmark)
{
  PyBookmark *self = bookmark_to_py_bookmark(bookmark);

  PyGILState_STATE gstate = PyGILState_Ensure();
  if (self->save)
    _py_invoke_void_function(self->save, self->data, "Bookmark", NULL);

  Py_XDECREF(self);
  PyGILState_Release(gstate);
}

void
py_bookmark_fill(Bookmark *bookmark, PyBookmark *py_bookmark)
{
  bookmark->save = py_bookmark_save;

  PyBookmarkData *data = (PyBookmarkData *) &bookmark->container;
  Py_XINCREF(py_bookmark);
  data->py_bookmark = py_bookmark;
}

static PyMemberDef
py_bookmark_members[] =
{
  { "data", T_OBJECT, offsetof(PyBookmark, data), 0 },
  { "save", T_OBJECT, offsetof(PyBookmark, save), 0 },
  {NULL}
};

PyTypeObject py_bookmark_type =
{
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  .tp_name = "Bookmark",
  .tp_basicsize = sizeof(PyBookmark),
  .tp_dealloc = (destructor) py_bookmark_free,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "Bookmark class encapsulating a syslog-ng bookmark",
  .tp_new = PyType_GenericNew,
  .tp_members = py_bookmark_members,
  0,
};

void
py_bookmark_init(void)
{
  PyType_Ready(&py_bookmark_type);
}
