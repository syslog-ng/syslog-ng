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
#include "python-ack-tracker.h"
#include "python-bookmark.h"
#include "apphook.h"
#include "cfg.h"
#include "ack-tracker/instant_ack_tracker.h"

#include "msg_parse_lib.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

static PyObject *_python_main;
static PyObject *_python_main_dict;

MsgFormatOptions parse_options;


static void
_py_init_interpreter(void)
{
  py_setup_python_home();
  Py_Initialize();
  py_init_argv();

  py_init_threads();
  py_ack_tracker_init();
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

  init_parse_options_and_load_syslogformat(&parse_options);

  _py_init_interpreter();
  _init_python_main();
}

void teardown(void)
{
  deinit_syslogformat_module();
  app_shutdown();
}

TestSuite(python_ack_tracker, .init = setup, .fini = teardown);

static PyObject *
ack_callback(PyObject *self, PyObject *args)
{
  Py_RETURN_NONE;
}

static PyMethodDef test_ack_callback =
{
  "ack_callback", ack_callback, METH_VARARGS, "Test ack callback"
};

struct AckTrackerFactoryTestParams
{
  PyTypeObject *ack_tracker_factory_type;
  AckTrackerType expected_ack_tracker_type;
};

Test(python_ack_tracker, test_instant_ack_tracker_factory)
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  PyObject *factory_args = Py_BuildValue("(N)", PyCFunction_New(&test_ack_callback, NULL));

  PyObject *py_ack_tracker_factory_obj = PyObject_CallObject((PyObject *) &py_instant_ack_tracker_factory_type,
                                                             factory_args);
  Py_XDECREF(factory_args);

  cr_assert_not_null(py_ack_tracker_factory_obj);
  cr_assert(py_is_ack_tracker_factory(py_ack_tracker_factory_obj));

  PyAckTrackerFactory *py_ack_tracker_factory = (PyAckTrackerFactory *) py_ack_tracker_factory_obj;
  cr_assert_not_null(py_ack_tracker_factory->ack_tracker_factory);
  cr_assert_eq(ack_tracker_factory_get_type(py_ack_tracker_factory->ack_tracker_factory),
               ACK_INSTANT);

  Py_XDECREF(py_ack_tracker_factory);
  PyGILState_Release(gstate);
}


Test(python_ack_tracker, test_consecutive_ack_tracker_factory)
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  PyObject *factory_args = Py_BuildValue("(N)", PyCFunction_New(&test_ack_callback, NULL));

  PyObject *py_ack_tracker_factory_obj = PyObject_CallObject((PyObject *) &py_consecutive_ack_tracker_factory_type,
                                                             factory_args);
  Py_XDECREF(factory_args);

  cr_assert_not_null(py_ack_tracker_factory_obj);
  cr_assert(py_is_ack_tracker_factory(py_ack_tracker_factory_obj));

  PyAckTrackerFactory *py_ack_tracker_factory = (PyAckTrackerFactory *) py_ack_tracker_factory_obj;
  cr_assert_not_null(py_ack_tracker_factory->ack_tracker_factory);
  cr_assert_eq(ack_tracker_factory_get_type(py_ack_tracker_factory->ack_tracker_factory),
               ACK_CONSECUTIVE);

  Py_XDECREF(py_ack_tracker_factory);
  PyGILState_Release(gstate);
}
