/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 László Várady
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include "python-helpers.h"
#include "python-bookmark.h"
#include "apphook.h"
#include "ack-tracker/bookmark.h"

#include <criterion/criterion.h>

static PyObject *_python_main;
static PyObject *_python_main_dict;

const gchar *test_bookmark_data = "test-bookmark-data";
const gchar *bookmark_saved_marker = "bookmark-saved";

static void
_py_init_interpreter(void)
{
  py_setup_python_home();
  Py_Initialize();
  py_init_argv();

  py_init_threads();
  py_bookmark_init();
  PyEval_SaveThread();
}

static void
_init_python_main(void)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  {
    _python_main = PyImport_AddModule("__main__");
    _python_main_dict = PyModule_GetDict(_python_main);
  }
  PyGILState_Release(gstate);
}

void setup(void)
{
  app_startup();

  _py_init_interpreter();
  _init_python_main();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(python_bookmark, .init = setup, .fini = teardown);

static PyObject *
test_save_bookmark(PyObject *self, PyObject *args)
{
  PyObject *bookmark_data;
  cr_assert(PyArg_ParseTuple(args, "O", &bookmark_data));
  cr_assert(PyBytes_Check(bookmark_data));
  cr_assert_str_eq(PyBytes_AsString(bookmark_data), test_bookmark_data);

  PyList_Append(self, bookmark_data);

  Py_RETURN_NONE;
}

static PyMethodDef test_save_method =
{
  "test_save_bookmark", test_save_bookmark, METH_VARARGS, "Test Bookmark::save()"
};


Test(python_bookmark, test_bookmark_saving)
{
  Bookmark bookmark = {0};
  PyBookmark *py_bookmark;
  PyObject *saved_bookmarks;

  {
    PyGILState_STATE gstate = PyGILState_Ensure();
    saved_bookmarks = PyList_New(0);

    PyObject *data = PyBytes_FromString(test_bookmark_data);
    PyObject *save = PyCFunction_New(&test_save_method, saved_bookmarks);

    py_bookmark = py_bookmark_new(data, save);
    cr_assert(py_is_bookmark((PyObject *) py_bookmark));

    py_bookmark_fill(&bookmark, py_bookmark);

    Py_CLEAR(data);
    Py_CLEAR(save);
    PyGILState_Release(gstate);
  }

  bookmark_save(&bookmark);

  {
    PyGILState_STATE gstate = PyGILState_Ensure();

    cr_assert_eq(PyList_Size(saved_bookmarks), 1);

    Py_CLEAR(py_bookmark);
    Py_CLEAR(saved_bookmarks);
    PyGILState_Release(gstate);
  }
}
